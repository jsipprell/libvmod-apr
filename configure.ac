AC_PREREQ([2.59])
AC_COPYRIGHT([Copyright (c) 2014 Jesse Sipprell])
AC_INIT([libvmod-apr], [0.1])
AC_CONFIG_MACRO_DIR(m4)
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_SRCDIR(src)
AM_CONFIG_HEADER([config.h])

AC_CANONICAL_SYSTEM
AC_LANG(C)

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_SILENT_RULES([yes])

AC_PROG_CC
AC_GNU_SOURCE
LT_INIT([disable-static])

dnl AC_PROG_CC_STDC
if test "x$ac_cv_prog_cc_c99" = xno; then
  AC_MSG_ERROR([Could not find a C99 compatible compiler])
fi
AC_PROG_CPP

AC_PROG_INSTALL
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

# Check for pkg-config
PKG_PROG_PKG_CONFIG

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([sys/stdlib.h])

# Check for python
AC_CHECK_PROGS(PYTHON, [python3 python3.1 python3.2 python2.7 python2.6 python2.5 python2 python],
  [AC_MSG_ERROR([Python is needed to build this vmod, please install python.])])

# Varnish source tree
AC_ARG_WITH([varnish],
  [AS_HELP_STRING([--with-varnish=DIR],
                 [location of varnish source code @<:@REQUIRED@:>@])],
  [varnish_srcdir="$withval"
   VARNISHSRC=])

AC_ARG_VAR([VARNISHSRC], [path to Varnish source tree (mandatory)])
AS_VAR_IF([VARNISHSRC],[""],[],[varnish_srcdir="$VARNISHSRC"])
AS_VAR_IF([varnish_srcdir],[""],[AC_MSG_ERROR([No Varnish source tree specified])])

VARNISHSRC=`cd $varnish_srcdir && pwd`
AC_CHECK_FILE([$varnish_srcdir/include/varnishapi.h],
  [],
  [AC_MSG_FAILURE(["$varnish_srcdir" is not a Varnish source directory])]
)

# vmod installation dir
AC_ARG_VAR([VMODDIR], [vmod installation directory @<:@LIBDIR/varnish/vmods@:>@])
if test "x$VMODDIR" = x; then
  VMODDIR=`pkg-config --variable=vmoddir varnishapi`
  if test "x$VMODDIR" = x; then
    AC_MSG_FAILURE([Can't determine vmod installation directory])
  fi
fi
AC_SUBST([VARNISHSRC],[$varnish_srcdir])
AC_OUTPUT([Makefile src/Makefile])