/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit vmod.vcc and run vmod.py instead
 */

#include "vrt.h"
#include "vcc_if.h"
#include "vmod_abi.h"


typedef void td_apr_init(struct sess *, struct vmod_priv *);
typedef const char * td_apr_get(struct sess *, struct vmod_priv *, const char *);
typedef void td_apr_set(struct sess *, struct vmod_priv *, const char *, const char *);
typedef void td_apr_del(struct sess *, struct vmod_priv *, const char *);
typedef void td_apr_format(struct sess *, struct vmod_priv *, const char *, const char *, ...);

const char Vmod_Name[] = "apr";
const struct Vmod_Func_apr {
	td_apr_init	*init;
	td_apr_get	*get;
	td_apr_set	*set;
	td_apr_del	*del;
	td_apr_format	*format;
	vmod_init_f	*_init;
} Vmod_Func = {
	vmod_init,
	vmod_get,
	vmod_set,
	vmod_del,
	vmod_format,
	init_function,
};

const int Vmod_Len = sizeof(Vmod_Func);

const char Vmod_Proto[] =
	"typedef void td_apr_init(struct sess *, struct vmod_priv *);\n"
	"typedef const char * td_apr_get(struct sess *, struct vmod_priv *, const char *);\n"
	"typedef void td_apr_set(struct sess *, struct vmod_priv *, const char *, const char *);\n"
	"typedef void td_apr_del(struct sess *, struct vmod_priv *, const char *);\n"
	"typedef void td_apr_format(struct sess *, struct vmod_priv *, const char *, const char *, ...);\n"
	"\n"
	"struct Vmod_Func_apr {\n"
	"	td_apr_init	*init;\n"
	"	td_apr_get	*get;\n"
	"	td_apr_set	*set;\n"
	"	td_apr_del	*del;\n"
	"	td_apr_format	*format;\n"
	"	vmod_init_f	*_init;\n"
	"} Vmod_Func_apr;\n"
	;

const char * const Vmod_Spec[] = {
	"apr.init\0Vmod_Func_apr.init\0VOID\0PRIV_VCL\0",
	"apr.get\0Vmod_Func_apr.get\0STRING\0PRIV_VCL\0STRING\0",
	"apr.set\0Vmod_Func_apr.set\0VOID\0PRIV_VCL\0STRING\0STRING\0",
	"apr.del\0Vmod_Func_apr.del\0VOID\0PRIV_VCL\0STRING\0",
	"apr.format\0Vmod_Func_apr.format\0VOID\0PRIV_VCL\0STRING\0STRING_LIST\0",
	"INIT\0Vmod_Func_apr._init",
	0
};
const char Vmod_Varnish_ABI[] = VMOD_ABI_Version;
const void * const Vmod_Id = &Vmod_Id;

