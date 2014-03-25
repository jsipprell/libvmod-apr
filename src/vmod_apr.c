#include "vmod_apr.h"

#define LOG_E(...) fprintf(stderr, __VA_ARGS__);
#ifdef DEBUG
# define LOG_T(...) fprintf(stderr, __VA_ARGS__);
#else
# define LOG_T(...) do {} while(0);
#endif

typedef struct {
  apr_pool_t *pool,*global_pool;
  apr_thread_mutex_t *mutex;
  volatile apr_uint32_t *refcount;
} apr_config_t;

static apr_status_t indirect_wipe(void *pptr)
{
  if(pptr)
    *((void**)pptr) = NULL;
  return APR_SUCCESS;
}

static apr_status_t cleanup_config(void *cfgptr)
{
  apr_config_t *cfg = (apr_config_t*)cfgptr;

  cfg->pool = cfg->global_pool = NULL;
  cfg->refcount = NULL;
  cfg->mutex = NULL;
  return APR_SUCCESS;
}

static apr_config_t *make_config(void)
{
  static volatile apr_uint32_t refcount = 0;
  static apr_pool_t *global_pool = NULL;
  static apr_thread_mutex_t *global_mutex = NULL;
  apr_config_t *cfg;
  LOG_T("make config\n");

  if(apr_atomic_read32(&refcount) == 0 && global_pool == NULL) {
    static volatile apr_uint32_t spinlock = 0;

    while(apr_atomic_cas32(&spinlock,1,0) != 0)
      ;
    if(global_pool == NULL) {
      /* apr_atomic_set32(&refcount,1); */
      ASSERT_APR(apr_initialize());
      ASSERT_APR(apr_pool_create(&global_pool,NULL));
      AN(global_pool);

      ASSERT_APR(apr_thread_mutex_create(&global_mutex,APR_THREAD_MUTEX_NESTED,global_pool));
      AN(global_mutex);

      apr_pool_cleanup_register(global_pool,&global_mutex,
                                indirect_wipe,apr_pool_cleanup_null);
      apr_pool_cleanup_register(global_pool,&global_pool,
                                indirect_wipe,apr_pool_cleanup_null);
    }
    AZ(apr_atomic_dec32(&spinlock));
  }

  ASSERT_APR(apr_thread_mutex_lock(global_mutex));
  AN(cfg = apr_palloc(global_pool,sizeof(apr_config_t)));
  cfg->global_pool = global_pool;
  cfg->pool = NULL;
  cfg->mutex = global_mutex;
  cfg->refcount = &refcount;
  ASSERT_APR(apr_pool_create(&cfg->pool,global_pool));
  apr_pool_cleanup_register(cfg->pool,cfg,cleanup_config,apr_pool_cleanup_null);
  ASSERT_APR(apr_thread_mutex_unlock(global_mutex));
  apr_atomic_inc32(cfg->refcount);
  LOG_T("config refcount: %u\n",(unsigned)apr_atomic_read32(&refcount));

  return cfg;
}

static void free_config(apr_config_t *cfg)
{
  static volatile apr_uint32_t *refcount = NULL;
  static apr_pool_t *global_pool = NULL;

#ifdef DEBUG
  if(refcount) {
    LOG_T("free_config, start refcount %u\n",(unsigned)apr_atomic_read32(refcount));
  } else {
    LOG_T("free_config, unknown refcount (yet)\n");
  }
#endif

  if (refcount == NULL && cfg && cfg->refcount) {
    static volatile apr_uint32_t spinlock = 0;
    while(apr_atomic_cas32(&spinlock,0,1) != 0)
      ;
    if(refcount == NULL)
      refcount = cfg->refcount;
    assert(apr_atomic_cas32(&spinlock,1,0) == 1);
    ASSERT_APR(apr_thread_mutex_unlock(cfg->mutex));
    LOG_T("free_config, got refcount %u\n",(unsigned)apr_atomic_read32(refcount));
  }
  if (cfg) {
    apr_pool_t *p = cfg->pool;
    apr_thread_mutex_t *mutex = cfg->mutex;

    if(cfg->global_pool != global_pool)
      global_pool = cfg->global_pool;

    if(p) {
      ASSERT_APR(apr_thread_mutex_lock(mutex));
      apr_pool_destroy(p);
      ASSERT_APR(apr_thread_mutex_unlock(mutex));
    }
  }
  if(!refcount || apr_atomic_dec32(refcount) == 0) {
    apr_pool_t *p = global_pool;
    LOG_T("free_config: refcount %u, terminating apr.\n",(unsigned)apr_atomic_read32(refcount));
    global_pool = NULL;
    if(p != NULL)
      apr_pool_destroy(p);
    apr_terminate();
  } else {
    LOG_T("free_config finished, refcount %u\n",(unsigned)apr_atomic_read32(refcount));
  }
}

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
  apr_config_t *p = priv->priv;

  LOG_T("apr init\n");
  if(p) {
    LOG_T("double init? priv already exists!\n");
    priv->priv = NULL;
    free_config(p);
  }
  priv->priv = make_config();
  priv->free = (vmod_priv_free_f*)free_config;

  return 0;
}

void vmod_init(struct sess *sp, struct vmod_priv *priv)
{
  if(priv->priv)
    free_config(priv->priv);
  priv->priv = make_config();
}
