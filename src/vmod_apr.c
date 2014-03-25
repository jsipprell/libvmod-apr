#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#define LOG_E(...) fprintf(stderr, __VA_ARGS__);
#ifdef DEBUG
# define LOG_T(...) fprintf(stderr, __VA_ARGS__);
#else
# define LOG_T(...) do {} while(0);
#endif

typedef struct {
  void *pool;
} apr_config_t;

static apr_config_t *make_config(void)
{
  apr_config_t *cfg;
  LOG_T("make config\n");

  cfg = malloc(sizeof(apr_config_t));
  if(cfg != NULL) {
    cfg->pool = malloc(1024);
    assert(cfg->pool != NULL);
  }
  return cfg;
}

static void free_config(apr_config_t *cfg)
{
  if(cfg) {
    if(cfg->pool) {
      free(cfg->pool);
      cfg->pool = NULL;
    }
    free(cfg);
  }
}

int init_function(struct vmod_priv *priv, const struct VCL_conf *conf)
{
  LOG_T("apr init\n");
  priv->free = (vmod_priv_free_f*)free_config;
  return 0;
}

void vmod_init(struct sess *sp, struct vmod_priv *priv)
{
  if(priv->priv)
    free_config(priv->priv);
  priv->priv = make_config();
}
