include $(top_srcdir)/build/modmake.rulesam
MODNAME=mod_audio_docker

mod_LTLIBRARIES = mod_audio_docker.la
mod_audio_docker_la_SOURCES  = mod_audio_docker.c lws_audio_docker_glue.cpp parser.cpp audio_pipe.cpp
mod_audio_docker_la_CFLAGS   = $(AM_CFLAGS)
mod_audio_docker_la_CXXFLAGS = $(AM_CXXFLAGS) -std=c++11

mod_audio_docker_la_LIBADD   = $(switch_builddir)/libfreeswitch.la
mod_audio_docker_la_LDFLAGS  = -avoid-version -module -no-undefined -shared `pkg-config --libs libwebsockets` 
