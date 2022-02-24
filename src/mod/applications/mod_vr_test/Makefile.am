include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vr_test

mod_LTLIBRARIES = mod_vr_test.la
mod_vr_test_la_SOURCES  = mod_vr_test.c
mod_vr_test_la_CFLAGS   = $(AM_CFLAGS)
mod_vr_test_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_vr_test_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
