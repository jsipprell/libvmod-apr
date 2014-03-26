#include "vmod_apr.h"

#define LOG_E(...) fprintf(stderr, __VA_ARGS__);
#ifdef DEBUG
# define LOG_T(...) fprintf(stderr, __VA_ARGS__);
#else
# define LOG_T(...) do {} while(0);
#endif


#ifdef DEBUG
#define begin_csession begin_csession_debug
#else
#define begin_csession(sp,priv,cfgp,fn_name) begin_csession_ndebug((sp),(priv),(cfgp))
#endif /* DEBUG */

#define CLEANUP(p,v,f) apr_pool_cleanup_register((p),(v),(f),apr_pool_cleanup_null)
#define KILL_CLEANUP(p,v,f) apr_pool_cleanup_kill((p),(v),(f))
#define CLEANUP_RUN(p,v,f) apr_pool_cleanup_run((p),(v),(f))
#define INDIRECT_CLEANUP(p,v) CLEANUP((p),&(v),indirect_wipe)
#define KILL_INDIRECT_CLEANUP(p,v) KILL_CLEANUP((p),&(v),indirect_wipe)

#define BEGIN_SESSION(sp) WS_Reserve((sp)->wrk->ws,0); do
#define BEGIN_CSESSION(sp,priv,cfg,decl) BEGIN_SESSION(sp) { \
    decl = begin_csession((sp),(priv),&(cfg),NULL); do
#define BEGIN_CSESSION_FN(fn,sp,priv,cfg,decl) BEGIN_SESSION(sp) { \
    decl = begin_csession((sp),(priv),&(cfg),(fn)); do

#define END_SESSION(sp) while(0); WS_Release((sp)->wrk->ws,0)
#define END_CSESSION(sp,cfg,s) while(0); end_csession(cfg,s); } END_SESSION(sp)

static apr_status_t indirect_wipe(void*);

typedef struct {
  apr_pool_t *pool;
  apr_thread_mutex_t *mutex;
  volatile apr_uint32_t refcount;
  apr_array_header_t *sessions;
} apr_config_t;

typedef struct {
  apr_pool_t *pool;
  apr_thread_mutex_t *mutex;
  apr_int64_t id,xid;
} apr_session_t;

static apr_pool_t *global_pool = NULL;
static apr_thread_mutex_t *global_mutex = NULL;
static volatile apr_uint32_t initialized = 0;
static volatile apr_uint32_t total_configs = 0;

static inline void set_empty_session(apr_session_t *s, apr_pool_t *pool)
{
  s->pool = NULL;
  s->mutex = NULL;
  s->id = s->xid = APR_INT64_C(-1);

  if (s->mutex == NULL) {
    ASSERT_APR(apr_thread_mutex_create(&s->mutex,APR_THREAD_MUTEX_DEFAULT,pool));
    INDIRECT_CLEANUP(pool,s->mutex);
  }
}

static apr_int64_t set_session(const struct sess *sp, apr_session_t *s, apr_config_t *cfg,
                                                                              int locked)
{
  apr_int64_t i = APR_INT64_C(-1);

  AN(s->mutex);
  ASSERT_APR(apr_thread_mutex_lock(s->mutex));
  if(s->xid > APR_INT64_C(-1) && sp && s->xid == sp->xid) {
    assert(s->id == sp->id);
    LOG_T("session reuse (0x%" APR_UINT64_T_HEX_FMT ")\n",s->xid);
    if(s->pool == NULL) {
      AN(cfg);
      AN(cfg->pool);
      LOG_T("session's pool has been destroyed, creating a new one for xid 0x%" APR_UINT64_T_HEX_FMT "\n",
            (apr_uint64_t)sp->xid);
      ASSERT_APR(apr_pool_create(&s->pool,cfg->pool));
      INDIRECT_CLEANUP(s->pool,s->pool);
    }
    i = s->id;
    if(!locked)
      ASSERT_APR(apr_thread_mutex_unlock(s->mutex));
    return i;
  } else {
    LOG_T("session mismatch, discarding (0x%" APR_UINT64_T_HEX_FMT " != 0x%"
          APR_UINT64_T_HEX_FMT ")\n",s->xid,(apr_uint64_t)sp->xid)
  }

  if(s->pool) {
    KILL_INDIRECT_CLEANUP(s->pool,s->pool);
    apr_pool_clear(s->pool);
  } else {
    AN(cfg);
    AN(cfg->pool);
    ASSERT_APR(apr_pool_create(&s->pool,cfg->pool));
  }
  INDIRECT_CLEANUP(s->pool,s->pool);
  if(sp) {
    i = s->id = sp->id;
    s->xid = sp->xid;
  } else {
    s->id = s->xid = i;
  }

  if(!locked)
    ASSERT_APR(apr_thread_mutex_unlock(s->mutex));
  return i;
}

static apr_session_t *get_session(const struct sess *sp, apr_config_t *cfg, int locked)
{
  apr_int64_t i = APR_INT64_C(-1);
  apr_session_t *s;

  ASSERT_APR(apr_thread_mutex_lock(global_mutex));

  if(cfg->sessions->nelts <= sp->id) {
    while(cfg->sessions->nelts <= sp->id) {
      set_empty_session(&APR_ARRAY_PUSH(cfg->sessions,apr_session_t),cfg->pool);
    }
  }

  assert(cfg->sessions->nelts > sp->id);
  s = &APR_ARRAY_IDX(cfg->sessions,sp->id,apr_session_t);
  i = set_session(sp,s,cfg,locked);
  assert(i == sp->id);
  ASSERT_APR(apr_thread_mutex_unlock(global_mutex));
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
    assert(rc < 1);
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

#ifdef DEBUG
static inline apr_session_t *begin_csession_debug(struct sess *sp, struct vmod_priv *priv,
                                                            apr_config_t **cfgp,
                                                            const char *fn_name)
#else
static inline apr_session_t *begin_csession_ndebug(struct sess *sp, struct vmod_priv *priv,
                                                            apr_config_t **cfgp)
#endif
{
  apr_config_t *cfg;

  if(!priv->priv) {
#ifdef DEBUG
    LOG_T("%s: no priv->priv\n",fn_name);
#endif
    priv->priv = make_config();
  }
#ifdef DEBUG
  if(!priv->free) {
    LOG_T("%s: no priv->free\n",fn_name);
    priv->free = (vmod_priv_free_f*)free_config;
  }
#endif
  AN(cfg = (apr_config_t*)priv->priv);
  if(cfgp)
    *cfgp = cfg;
  apr_atomic_inc32(&cfg->refcount);
#ifdef DEBUG
  do {
    apr_session_t *s = get_session(sp,cfg,1);
    AN(s);
    return s;
  } while(0);
#else
  return get_session(sp,cfg,1);
#endif
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static inline void end_session(struct vmod_priv *priv)
{
  AN(priv->priv);
  apr_atomic_dec32(&((apr_config_t*)priv->priv)->refcount);
}
#pragma GCC diagnostic pop

static inline void end_csession(apr_config_t *cfg, apr_session_t *s)
{
  AN(cfg);
  apr_atomic_dec32(&cfg->refcount);
  AN(s);
  if(s->mutex) {
    ASSERT_APR(apr_thread_mutex_unlock(s->mutex));
  }
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
  void *val = NULL;

  BEGIN_CSESSION_FN("vmod_get",sp,priv,cfg,apr_session_t *s) {
    if(s->pool) {
      LOG_T("vmod_get: using session pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_get: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }

    ASSERT_APR(apr_pool_userdata_get(&val,key,pool));
  } END_CSESSION(sp,cfg,s);
  return val;
}

void vmod_set(struct sess *sp, struct vmod_priv *priv, const char *key, const char *v, ...)
{
  va_list ap;
  apr_config_t *cfg;
  apr_pool_t *pool;

  BEGIN_CSESSION_FN("vmod_set",sp,priv,cfg,apr_session_t *s) {
    char buf[8192], *cp, *op;

    if(s->pool) {
      LOG_T("vmod_set: using session pool 0x%" APR_UINT64_T_HEX_FMT ", refcount=%u\n",
            (apr_uint64_t)s->pool,apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_set: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }


    if (v == NULL) {
      ASSERT_APR(apr_pool_userdata_set(apr_pstrdup(pool,"(null)"),key,NULL,pool));
    } else if (v != NULL && v != vrt_magic_string_unset) {
      op = cp = buf;
      cp = apr_cpystrn(op,v,sizeof(buf));
      if(cp && *cp == '\0') {
        char *arg;
        unsigned cnt = 0;
        va_start(ap,v);
        for(arg = va_arg(ap,char*); arg != vrt_magic_string_end && ((cp-op) < sizeof(buf)-1) && *cp == '\0';
                                    arg = va_arg(ap,char*)) {
          cp = apr_cpystrn(cp,arg ? arg : "(null)",(sizeof(buf)-(cp-op)));
          cnt++;
        }
        va_end(ap);
        assert(*cp == '\0');
        ASSERT_APR(apr_pool_userdata_set(apr_pstrdup(pool,buf),key,NULL,pool));
      }
    }
  } END_CSESSION(sp,cfg,s);
}

void vmod_del(struct sess *sp, struct vmod_priv *priv, const char *key)
{
  apr_config_t *cfg;
  apr_pool_t *pool;

  BEGIN_CSESSION_FN("vmod_del",sp,priv,cfg,apr_session_t *s) {
    if(s->pool) {
      LOG_T("vmod_del: using session pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = s->pool;
    } else {
      LOG_T("vmod_del: using GLOBAL configuration pool, refcount=%u\n",apr_atomic_read32(&cfg->refcount));
      pool = cfg->pool;
    }

    ASSERT_APR(apr_pool_userdata_setn(NULL,key,NULL,pool));
  } END_CSESSION(sp,cfg,s);
}

void vmod_destroy(struct sess *sp, struct vmod_priv *priv)
{
  apr_config_t *cfg;
  BEGIN_CSESSION_FN("vmod_destroy",sp,priv,cfg,apr_session_t *s) {
    if(s->pool) {
      LOG_T("destroying session pool for xid 0x%" APR_UINT64_T_HEX_FMT "\n",(apr_uint64_t)sp->xid);
      apr_pool_destroy(s->pool);
    }
  } END_CSESSION(sp,cfg,s);
}

void vmod_clear(struct sess *sp, struct vmod_priv *priv)
{
  apr_config_t *cfg;
  BEGIN_CSESSION_FN("vmod_destroy",sp,priv,cfg,apr_session_t *s) {
    if(s->pool) {
      LOG_T("clearing session pool for xid 0x%" APR_UINT64_T_HEX_FMT "\n",(apr_uint64_t)sp->xid);
      KILL_INDIRECT_CLEANUP(s->pool,s->pool);
      apr_pool_clear(s->pool);
      INDIRECT_CLEANUP(s->pool,s->pool);
    }
  } END_CSESSION(sp,cfg,s);
}

