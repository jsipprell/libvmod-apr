/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit vmod.vcc and run vmod.py instead
 */

struct sess;
struct VCL_conf;
struct vmod_priv;

void vmod_init(struct sess *, struct vmod_priv *);
const char * vmod_get(struct sess *, struct vmod_priv *, const char *);
void vmod_set(struct sess *, struct vmod_priv *, const char *, const char *, ...);
void vmod_del(struct sess *, struct vmod_priv *, const char *);
void vmod_destroy(struct sess *, struct vmod_priv *);
void vmod_clear(struct sess *, struct vmod_priv *);
int init_function(struct vmod_priv *, const struct VCL_conf *);
extern const void * const Vmod_Id;
