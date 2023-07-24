include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_azure_tts

mod_LTLIBRARIES = mod_azure_tts.la
mod_azure_tts_la_SOURCES  = mod_azure_tts.c azure_glue.cpp
mod_azure_tts_la_CFLAGS   = -I$(AZURE_PATH)/include/c_api -I$(AZURE_PATH)/include/cxx_api $(AM_CFLAGS) -std=c99
mod_azure_tts_la_CXXFLAGS = -I$(AZURE_PATH)/include/c_api -I$(AZURE_PATH)/include/cxx_api $(AM_CXXFLAGS) -std=c++14

mod_azure_tts_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_azure_tts_la_LDFLAGS  = -avoid-version -module -no-undefined -L/usr/local/src/freeswitch/libs/speechsdk/lib/x64 -lMicrosoft.CognitiveServices.Speech.core -shared `pkg-config --libs libwebsockets` 
