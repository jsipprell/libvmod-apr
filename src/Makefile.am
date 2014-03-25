AM_CPPFLAGS = -I$(VARNISHSRC)/include -I$(VARNISHSRC)

vmoddir = $(VMODDIR)
vmod_LTLIBRARIES = libvmod_apr.la

libvmod_apr_la_CPPFLAGS = @APR_CPPFLAGS@ $(AM_CPPFLAGS)
libvmod_apr_la_CFLAGS = @APR_CFLAGS@
libvmod_apr_la_LDFLAGS = -module -export-dynamic -avoid-version
libvmod_apr_la_LIBADD = @APR_LIBS@ @APR_UTIL_LIBS@
libvmod_apr_la_SOURCES = \
  vcc_if.c \
  vcc_if.h \
  vmod_apr.h \
  vmod_apr.c


vcc_if.c vcc_if.h: $(top_builddir)/build/vmod.py $(srcdir)/vmod_apr.vcc
	@PYTHON@ $(top_builddir)/build/vmod.py $(srcdir)/vmod_apr.vcc

EXTRA_DIST = vmod_apr.vcc

CLEANFILES = $(builddir)/vcc_if.c $(builddir)/vcc_if.h
