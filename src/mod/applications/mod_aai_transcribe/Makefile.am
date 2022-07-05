include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_aai_transcribe

mod_LTLIBRARIES = mod_aai_transcribe.la
mod_aai_transcribe_la_SOURCES  = mod_aai_transcribe.c aai_connector.cpp
mod_aai_transcribe_la_CFLAGS   = $(AM_CFLAGS)
mod_aai_transcribe_la_CXXFLAGS = -I $(top_srcdir)/libs/googleapis/gens $(AM_CXXFLAGS) -std=c++11

mod_aai_transcribe_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_aai_transcribe_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `pkg-config --libs grpc++ grpc` 
