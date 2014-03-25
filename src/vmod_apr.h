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
#include <apr_errno.h>
#include <apr_atomic.h>
#include <apr_thread_proc.h>
#include <apr_thread_mutex.h>
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

#ifdef WITHOUT_ASSERTS
#define ASSERT_APR(op) ((void)(op))
#else
#define ASSERT_APR(op) do { \
  apr_status_t st = (op); \
  if(st != APR_SUCCESS) \
    VAS_Fail(__func__, __FILE__, __LINE__, #op, APR_TO_OS_ERROR(st), 0); \
} while(0)
#endif /* WITHOUT_ASSERTS */

#endif /* LIBVMOD_APR_H */