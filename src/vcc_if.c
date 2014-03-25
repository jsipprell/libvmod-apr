/*
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit vmod.vcc and run vmod.py instead
 */

#include "vrt.h"
#include "vcc_if.h"
#include "vmod_abi.h"


typedef void td_apr_init(struct sess *, struct vmod_priv *);

const char Vmod_Name[] = "apr";
const struct Vmod_Func_apr {
	td_apr_init	*init;
	vmod_init_f	*_init;
} Vmod_Func = {
	vmod_init,
	init_function,
};

const int Vmod_Len = sizeof(Vmod_Func);

const char Vmod_Proto[] =
	"typedef void td_apr_init(struct sess *, struct vmod_priv *);\n"
	"\n"
	"struct Vmod_Func_apr {\n"
	"	td_apr_init	*init;\n"
	"	vmod_init_f	*_init;\n"
	"} Vmod_Func_apr;\n"
	;

const char * const Vmod_Spec[] = {
	"apr.init\0Vmod_Func_apr.init\0VOID\0PRIV_VCL\0",
	"INIT\0Vmod_Func_apr._init",
	0
};
const char Vmod_Varnish_ABI[] = VMOD_ABI_Version;
const void * const Vmod_Id = &Vmod_Id;

