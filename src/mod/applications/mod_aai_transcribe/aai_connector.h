#ifndef __AAI_CONNECTOR_H__
#define __AAI_CONNECTOR_H__

switch_status_t aai_speech_init();
switch_status_t aai_speech_cleanup();
switch_status_t aai_speech_session_init(switch_core_session_t *session, responseHandler_t responseHandler, 
		uint32_t samples_per_second, uint32_t channels, char* lang, int interim, int single_utterence, int sepreate_recognition,
		int max_alternatives, int profinity_filter, int word_time_offset, int punctuation, char* model, int enhanced, 
		char* hints, char* play_file, void **ppUserData);
switch_status_t aai_speech_session_cleanup(switch_core_session_t *session, int channelIsClosing);
switch_bool_t aai_speech_frame(switch_media_bug_t *bug, void* user_data);

#endif