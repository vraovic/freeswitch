#ifndef __MOD_AAI_TRANS_H__
#define __MOD_AAI_TRANS_H__

#include <switch.h>
#include <libwebsockets.h>
#include <speex/speex_resampler.h>

#include <unistd.h>

#define MY_BUG_NAME "audio_aai_transcription"
#define MAX_SESSION_ID (256)
#define MAX_WS_URL_LEN (512)
#define MAX_PATH_LEN (4096)

#define EVENT_TRANSCRIPTION   "mod_aai_transcription::transcription"
#define EVENT_ERROR           "mod_aai_transcription::error"
#define EVENT_CONNECT_SUCCESS "mod_aai_transcription::connect"
#define EVENT_CONNECT_FAIL    "mod_aai_transcription::connect_failed"
#define EVENT_BUFFER_OVERRUN  "mod_aai_transcription::buffer_overrun"
#define EVENT_JSON            "mod_aai_transcription::json"

#define MAX_METADATA_LEN (8192)

struct playout {
  char *file;
  struct playout* next;
};

typedef void (*responseHandler_t)(switch_core_session_t* session, const char* eventName, char* json);

struct private_data {
	switch_mutex_t *mutex;
	char sessionId[MAX_SESSION_ID];
  SpeexResamplerState *resampler;
  responseHandler_t responseHandler;
  void *pAudioPipe;
  int ws_state;
  char host[MAX_WS_URL_LEN];
  unsigned int port;
  char path[MAX_PATH_LEN];
  int sampling;
  struct playout* playout;
  int  channels;
  unsigned int id;
  int buffer_overrun_notified:1;
  int audio_paused:1;
  int graceful_shutdown:1;
  char initialMetadata[8533];
};

typedef struct private_data private_t;

#endif
