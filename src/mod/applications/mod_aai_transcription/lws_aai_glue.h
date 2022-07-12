#ifndef __LWS_AAI_GLUE_H__
#define __LWS_AAI_GLUE_H__

#include "mod_aai_transcription.h"

int parse_ws_uri(switch_channel_t *channel, const char* szServerUri, char* host, char *path, unsigned int* pPort, int* pSslFlags);

switch_status_t aai_init();
switch_status_t aai_cleanup();
switch_status_t aai_session_init(switch_core_session_t *session, responseHandler_t responseHandler,
		uint32_t samples_per_second, char *host, unsigned int port, char* path, int sampling, int sslFlags, int channels, void **ppUserData);
switch_status_t aai_session_cleanup(switch_core_session_t *session, char* text, int channelIsClosing);
switch_status_t aai_session_pauseresume(switch_core_session_t *session, int pause);
switch_status_t aai_session_graceful_shutdown(switch_core_session_t *session);
switch_status_t aai_session_send_text(switch_core_session_t *session, char* text);
switch_bool_t aai_frame(switch_core_session_t *session, switch_media_bug_t *bug);
switch_status_t aai_service_threads();
#endif
