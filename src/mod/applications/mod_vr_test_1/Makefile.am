include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_vr_test_1

mod_LTLIBRARIES = mod_vr_test_1.la
mod_vr_test_1_la_SOURCES  = mod_vr_test_1.c
mod_vr_test_1_la_CFLAGS   = $(AM_CFLAGS)
mod_vr_test_1_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_vr_test_1_la_LDFLAGS  = -avoid-version -module -no-undefined -shared
