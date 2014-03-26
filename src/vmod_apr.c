#include "vmod_apr.h"

#define LOG_E(...) fprintf(stderr, __VA_ARGS__);
#ifdef DEBUG
# define LOG_T(...) fprintf(stderr, __VA_ARGS__);
#else
# define LOG_T(...) do {} while(0);
#endif

#define CLEANUP(p,v,f) apr_pool_cleanup_register((p),(v),(f),apr_pool_cleanup_null)
#define KILL_CLEANUP(p,v,f) apr_pool_cleanup_kill((p),(v),(f))
#define CLEANUP_RUN(p,v,f) apr_pool_cleanup_run((p),(v),(f))
#define INDIRECT_CLEANUP(p,v) CLEANUP((p),&(v),indirect_wipe)
#define KILL_INDIRECT_CLEANUP(p,v) KILL_CLEANUP((p),&(v),indirect_wipe)

#define BEGIN_SESSION(sp) WS_Reserve((sp)->wrk->ws,0); do
#define END_SESSION(sp) while(0); WS_Release((sp)->wrk->ws,0)

static apr_status_t indirect_wipe(void*);

typedef struct {
  apr_pool_t *pool;
  apr_thread_mutex_t *mutex;
  volatile apr_uint32_t refcount;
  apr_array_header_t *sessions;
} apr_config_t;

typedef struct {
  apr_pool_t *pool;
  apr_int64_t id,xid;
} apr_session_t;

static apr_pool_t *global_pool = NULL;
static apr_thread_mutex_t *global_mutex = NULL;
static volatile apr_uint32_t initialized = 0;
static volatile apr_uint32_t total_configs = 0;

static inline void set_empty_session(apr_session_t *s)
{
  s->pool = NULL;
  s->id = s->xid = APR_INT64_C(-1);
}

static apr_status_t decr_refcount(void *c)
{
  if(c) {
    if(apr_atomic_dec32(&(((apr_config_t*)c))->refcount) == 0) {
      LOG_T("config refcount decremented to zero via session subpool\n");
    }
  }
  return APR_SUCCESS;
}

static apr_int64_t set_session(const struct sess *sp, apr_session_t *s, apr_config_t *cfg)
{
  apr_int64_t i = APR_INT64_C(-1);

  if(s->xid > APR_INT64_C(-1) && sp && s->xid == sp->xid) {
    assert(s->id == sp->id);
    LOG_T("session reuse (0x%" APR_UINT64_T_HEX_FMT ")\n",s->xid);
    return s->id;
  } else {
    LOG_T("session mismatch, discarding (0x%" APR_UINT64_T_HEX_FMT " != 0x%"
          APR_UINT64_T_HEX_FMT ")\n",s->xid,(apr_uint64_t)sp->xid)
  }

  if(s->pool) {
    KILL_INDIRECT_CLEANUP(s->pool,s->pool);
    apr_atomic_inc32(&cfg->refcount);
    apr_pool_clear(s->pool);
  } else {
    AN(cfg);
    AN(cfg->pool);
    ASSERT_APR(apr_pool_create(&s->pool,cfg->pool));
    apr_atomic_inc32(&cfg->refcount);
  }
  CLEANUP(s->pool,cfg,decr_refcount);
  INDIRECT_CLEANUP(s->pool,s->pool);
  if(sp) {
    i = s->id = sp->id;
    s->xid = sp->xid;
  } else {
    s->id = s->xid = i;
  }

  return i;
}

static apr_session_t *get_session(const struct sess *sp, apr_config_t *cfg)
{
  apr_int64_t i = APR_INT64_C(-1);
  apr_session_t *s;

  while(cfg->sessions->nelts <= sp->id) {
    set_empty_session(&APR_ARRAY_PUSH(cfg->sessions,apr_session_t));
  }

  assert(cfg->sessions->nelts > sp->id);
  s = &APR_ARRAY_IDX(cfg->sessions,sp->id,apr_session_t);
  i = set_session(sp,s,cfg);
  assert(i == sp->id);
  return s;
}

static apr_status_t indirect_wipe(void *pptr)
{
  if(pptr)
    *((void**)pptr) = NULL;
  return APR_SUCCESS;
}

static apr_status_t cleanup_config(void *cfgptr)
{
  apr_config_t *cfg = (apr_config_t*)cfgptr;

  cfg->pool = NULL;
  cfg->mutex = NULL;
  return APR_SUCCESS;
}

static apr_status_t finalize(void *last)
{
  apr_pool_t **p = (apr_pool_t**)last;

  LOG_T("FINALIZE\n");
  if(p && *p && *p == global_pool) {
    global_mutex = NULL;
    global_pool = NULL;
    LOG_T("APR TERMINATION\n");
    apr_terminate();
  } else {
    LOG_E("vmod_apr is unable to unload the Apache Portable Runtime as there are still resources in use. "
          "\nThis is an error.\n");
  }

  return APR_SUCCESS;
}

static void initialize(void)
{
  apr_uint32_t skip_unlock = 1;
  static volatile apr_uint32_t spinlock = 0;
  while((skip_unlock = apr_atomic_cas32(&spinlock,1,0)) != 0 && global_mutex == NULL)
    ;
  if (global_mutex == NULL) {
    AZ(global_pool);
    ASSERT_APR(apr_initialize());
    ASSERT_APR(apr_pool_create(&global_pool,NULL));
    ASSERT_APR(apr_thread_mutex_create(&global_mutex,APR_THREAD_MUTEX_DEFAULT,global_pool));
    CLEANUP(global_pool,&global_pool,finalize);
    apr_atomic_set32(&initialized,1);
  }
  if(!skip_unlock) {
    assert(apr_atomic_dec32(&spinlock) == 0);
  }
  while(apr_atomic_read32(&initialized) < 1)
    ;
}

static apr_config_t *make_config(void)
{
  apr_config_t *cfg = NULL;
  apr_pool_t *pool = NULL;
  if(apr_atomic_read32(&initialized) == 0)
    initialize();

  AN(global_pool);
  ASSERT_APR(apr_pool_create(&pool,global_pool));
  AN(pool);
  AN(cfg = apr_pcalloc(pool,sizeof(apr_config_t)));
  cfg->pool = pool;
  CLEANUP(pool,cfg,cleanup_config);
  ASSERT_APR(apr_thread_mutex_create(&cfg->mutex,APR_THREAD_MUTEX_NESTED,pool));
  AN(cfg->sessions = apr_array_make(pool,1000,sizeof(apr_session_t)));
  apr_atomic_inc32(&total_configs);
  LOG_T("vmod_apr: new config, total configs=%u\n",(unsigned)apr_atomic_read32(&total_configs));
  return cfg;
}

static void free_config(apr_config_t *cfg)
{
  if(apr_atomic_read32(&initialized) == 0)
    initialize();

  if(cfg) {
    apr_uint32_t rc = apr_atomic_read32(&cfg->refcount);
    assert(rc < 2);
    if(apr_atomic_dec32(&total_configs) == 0)
      apr_pool_destroy(global_pool);
    else {
      apr_pool_destroy(cfg->pool);
      LOG_T("vmod_apr: destroyed config, total configs=%u, final refcount=%u\n",
              (unsigned)apr_atomic_read32(&total_configs),
              (unsigned)rc);
    }
  } else {
    LOG_T("vmod_apr: WARNING unexpected NULL config release\n");
  }
}

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
  apr_config_t *p = priv->priv;

  if(apr_atomic_read32(&initialized) == 0)
    initialize();

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

const char *vmod_get(struct sess *sp, struct vmod_priv *priv, const char *key)
{
  apr_config_t *cfg;
  apr_pool_t *pool;
  apr_session_t *s;
  void *val = NULL;

  BEGIN_SESSION(sp) {
    if(!priv->priv) {
      LOG_T("vmod_get: no priv->priv\n");
      priv->priv = make_config();
    }
    if(!priv->free) {
      LOG_T("vmod_get: no priv->free\n");
      priv->free = (vmod_priv_free_f*)free_config;
    }

    AN(cfg = (apr_config_t*)priv->priv);
    AN(s = get_session(sp,cfg));
    if(s->pool) {
      LOG_T("vmod_get: using session pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_get: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }

    ASSERT_APR(apr_pool_userdata_get(&val,key,pool));
  } END_SESSION(sp);
  return val;
}

void vmod_set(struct sess *sp, struct vmod_priv *priv,
              const char *key, const char *val)
{
  apr_config_t *cfg;
  apr_pool_t *pool;
  apr_session_t *s;

  BEGIN_SESSION(sp) {
    if(!priv->priv) {
      LOG_T("vmod_set: no priv->priv\n");
      priv->priv = make_config();
    }
    if(!priv->free) {
      LOG_T("vmod_set: no priv->free\n");
      priv->free = (vmod_priv_free_f*)free_config;
    }

    AN(cfg = (apr_config_t*)priv->priv);
    AN(s = get_session(sp,cfg));
    if(s->pool) {
      LOG_T("vmod_set: using session pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_set: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }

    if(!val) val = "";
    ASSERT_APR(apr_pool_userdata_set(val,key,NULL,pool));
  } END_SESSION(sp);
}

void vmod_del(struct sess *sp, struct vmod_priv *priv, const char *key)
{
  apr_config_t *cfg;
  apr_pool_t *pool;
  apr_session_t *s;

  BEGIN_SESSION(sp) {
    if(!priv->priv) {
      LOG_T("vmod_del: no priv->priv\n");
      priv->priv = make_config();
    }
    if(!priv->free) {
      LOG_T("vmod_del: no priv->free\n");
      priv->free = (vmod_priv_free_f*)free_config;
    }

    AN(cfg = (apr_config_t*)priv->priv);
    AN(s = get_session(sp,cfg));
    if(s->pool) {
      LOG_T("vmod_del: using session pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_del: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }

    ASSERT_APR(apr_pool_userdata_setn(NULL,key,NULL,pool));
  } END_SESSION(sp);
}

void vmod_format(struct sess *sp, struct vmod_priv *priv, const char *key,
                                                          const char *fmt, ...)
{
  va_list ap;
  apr_config_t *cfg;
  apr_session_t *s;

  BEGIN_SESSION(sp) {
    char *val;

    AN(cfg = (apr_config_t*)priv->priv);
    AN(s = get_session(sp,cfg));
    AN(s->pool);
    va_start(ap,fmt);
    val = apr_pvsprintf(s->pool,fmt,ap);
    va_end(ap);
    AN(val);
    ASSERT_APR(apr_pool_userdata_set(val,key,NULL,s->pool));
  } END_SESSION(sp);
}