#ifndef LIBVMOD_APR_H
#define LIBVMOD_APR_H

#define APR_WANT_STRFUNC
#define APR_WANT_MEMFUNC
#define APR_WANT_STDIO
#define APR_WANT_BYTEFUNC

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <apr.h>
#include <apr_portable.h>
#include <apr_pools.h>
#include <apr_hash.h>
#include <apr_tables.h>
#include <apr_strings.h>
#include <apr_time.h>
#include <apr_general.h>
#include <apr_lib.h>
#include <apr_want.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_SYS_STDLIB_H
#include <sys/stdlib.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "vrt.h"
#include "bin/varnishd/cache.h"

#include "vcc_if.h"

#endif