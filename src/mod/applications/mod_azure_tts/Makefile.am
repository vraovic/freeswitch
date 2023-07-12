include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_azure_tts

mod_LTLIBRARIES = mod_azure_tts.la
mod_azure_tts_la_SOURCES  = mod_azure_tts.c azure_glue.cpp
mod_azure_tts_la_CFLAGS   = -I$(AZURE_PATH)/include/c_api  $(AM_CFLAGS)
mod_azure_tts_la_CXXFLAGS = -I$(AZURE_PATH)/include/cxx_api $(AM_CXXFLAGS) -std=c++11

mod_azure_tts_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_azure_tts_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `pkg-config --libs libwebsockets` 
