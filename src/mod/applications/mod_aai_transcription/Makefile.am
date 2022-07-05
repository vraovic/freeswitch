include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_aai_transcription

mod_LTLIBRARIES = mod_aai_transcription.la
mod_aai_transcription_la_SOURCES  = mod_aai_transcription.c lws_aai_glue.cpp parser.cpp audio_pipe.cpp
mod_aai_transcription_la_CFLAGS   = $(AM_CFLAGS)
mod_aai_transcription_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11

mod_aai_transcription_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_aai_transcription_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `pkg-config --libs libwebsockets` 
