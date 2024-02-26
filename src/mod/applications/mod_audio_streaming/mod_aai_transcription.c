/* 
 *
 * mod_aai_transcription.c -- Freeswitch module for streaming audio to AssemblyAI server over websockets
 *
 */
#include "mod_aai_transcription.h"
#include "lws_aai_glue.h"

//static int mod_running = 0;

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aai_transcription_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_aai_transcription_runtime);
SWITCH_MODULE_LOAD_FUNCTION(mod_aai_transcription_load);

SWITCH_MODULE_DEFINITION(mod_aai_transcription, mod_aai_transcription_load, mod_aai_transcription_shutdown, NULL /*mod_aai_transcription_runtime*/);

static void responseHandler(switch_core_session_t* session, const char * eventName, char * json) {
	switch_event_t *event;

	switch_channel_t *channel = switch_core_session_get_channel(session);
	if (json) 
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "responseHandler: ready to send eventName:%s - json: %s.\n",eventName, json);
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "responseHandler: ready to send eventName:%s - no json\n",eventName);

	}
	switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, eventName);
	switch_channel_event_set_data(channel, event);
	if (json) 
	{
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "responseHandler: generate SWITCH_EVENT_CUSTOM eventName:%s - json: %s\n",eventName, json);
		switch_event_add_body(event, "%s", json);
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "responseHandler: generate SWITCH_EVENT_CUSTOM eventName:%s\n",eventName);

	}
	switch_event_fire(&event);
}

static switch_bool_t capture_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_core_session_t *session = switch_core_media_bug_get_session(bug);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		break;

	case SWITCH_ABC_TYPE_CLOSE:
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Got SWITCH_ABC_TYPE_CLOSE.\n");
      		aai_session_cleanup(session, NULL, 1);
		}
		break;
	
	case SWITCH_ABC_TYPE_READ:
		return aai_frame(session, bug);
		break;

	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

static switch_status_t start_capture(switch_core_session_t *session, 
        switch_media_bug_flag_t flags, 
        char* host,
        unsigned int port, 
        char* path,
        int sampling,
        int sslFlags,
        const char* base)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_codec_t* read_codec;

	void *pUserData = NULL;
//   int channels = (flags & SMBF_STEREO) ? 2 : 1;
  	int channels = 1;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, 
    "mod_aai_transcription: streaming %d sampling to %s path %s port %d tls: %s.\n", 
    sampling, host, path, port, sslFlags ? "yes" : "no");

	if (switch_channel_get_private(channel, MY_BUG_NAME)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "mod_aai_transcription: bug already attached!\n");
		return SWITCH_STATUS_FALSE;
	}

	read_codec = switch_core_session_get_read_codec(session);

	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "mod_aai_transcription: channel must have reached pre-answer status before calling start!\n");
		return SWITCH_STATUS_FALSE;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "calling aai_session_init.\n");
	if (SWITCH_STATUS_FALSE == aai_session_init(session, responseHandler, read_codec->implementation->actual_samples_per_second, 
		host, port, path, sampling, sslFlags, channels, &pUserData)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error initializing mod_aai_transcription session.\n");
		return SWITCH_STATUS_FALSE;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "adding bug.\n");
	if ((status = switch_core_media_bug_add(session, MY_BUG_NAME, NULL, capture_callback, pUserData, 0, flags, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "setting bug private data.\n");
	switch_channel_set_private(channel, MY_BUG_NAME, bug);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "exiting start_capture.\n");
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t do_stop(switch_core_session_t *session, char* text)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (text) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "mod_aai_transcription: stop w/ final text %s\n", text);
	}
	else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "mod_aai_transcription: stop\n");
	}
	status = aai_session_cleanup(session, text, 0);

	return status;
}

static switch_status_t do_pauseresume(switch_core_session_t *session, int pause)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "mod_aai_transcription: %s\n", pause ? "pause" : "resume");
	status = aai_session_pauseresume(session, pause);

	return status;
}

static switch_status_t do_graceful_shutdown(switch_core_session_t *session)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	status = aai_session_graceful_shutdown(session);

	return status;
}

static switch_status_t send_text(switch_core_session_t *session, char* text) {
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug = switch_channel_get_private(channel, MY_BUG_NAME);

  if (bug) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "mod_aai_transcription: sending text: %s.\n", text);
    status = aai_session_send_text(session, text);
  }
  else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "mod_aai_transcription: no bug, failed sending text: %s.\n", text);
  }
  return status;
}

#define AAI_API_SYNTAX "<uuid> [start | stop ] [aai-url | path]"
SWITCH_STANDARD_API(aai_transcription_function)
{
	char *mycmd = NULL, *argv[4] = { 0 };
	int argc = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (!zstr(cmd) && (mycmd = strdup(cmd))) {
		argc = switch_separate_string(mycmd, ' ', argv, (sizeof(argv) / sizeof(argv[0])));
	}
	assert(cmd);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "mod_aai_transcription VR-cmd: %s argc:%u\n", cmd, argc);


	// if (zstr(cmd) || argc < 2 ||
	// 	(0 == strcmp(argv[1], "start") && argc < 5)) {

	// 	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error with command %s %s %s.\n", cmd, argv[0], argv[1]);
	// 	stream->write_function(stream, "-USAGE: %s\n", AAI_API_SYNTAX);
	// 	goto done;
	// } 
	// else 
	{
		switch_core_session_t *lsession = NULL;

		if ((lsession = switch_core_session_locate(argv[0]))) 
		{
			if (!strcasecmp(argv[1], "stop")) 
			{
				status = do_stop(lsession, argc > 2 ? argv[2] : NULL);
      		} 
			else if (!strcasecmp(argv[1], "pause")) 
			{
				status = do_pauseresume(lsession, 1);
      		}
			else if (!strcasecmp(argv[1], "resume")) 
			{
				status = do_pauseresume(lsession, 0);
      		}
			else if (!strcasecmp(argv[1], "graceful-shutdown")) 
			{
				status = do_graceful_shutdown(lsession);
      		}
      		else if (!strcasecmp(argv[1], "send_text")) 
			{
        		if (argc < 3) 
				{
          			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "send_text requires an argument specifying text to send\n");
          			switch_core_session_rwunlock(lsession);
          			goto done;
        		}
        		status = send_text(lsession, argv[2]);
      		}
      		else if (!strcasecmp(argv[1], "start")) 
			{
				switch_channel_t *channel = switch_core_session_get_channel(lsession);
        		char host[MAX_WS_URL_LEN], path[MAX_PATH_LEN], path1[MAX_PATH_LEN];
        		unsigned int port;
        		int sslFlags;
        		int sampling = 16000;
      			switch_media_bug_flag_t flags = SMBF_READ_STREAM ;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "argv[2]: %s path-size:%lu\n", argv[2], sizeof(path));

        		if (!parse_ws_uri(channel, argv[2], &host[0], &path[0], &port, &sslFlags)) 
				{
          			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "invalid websocket uri: %s\n", argv[2]);
        		}
        		else 
				{
					// Extract sampling rate from the path
					char *token =NULL;
    				char *next_token =NULL;
					char *token1 = NULL;
    				// char *next_token1 =NULL;
					strcpy(path1, path);
          			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Make TOKENS - copy to path1:%s\n",path1);
					token = strtok(path1, "&");
					token1 = strtok(token,"=");
        			next_token = strtok(NULL, "=");
					sampling = atoi(next_token);

          			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "TOKEN:%s, NEXT_TOKEN:%s, token1: %s, sampling:%d\n",token, next_token,token1,sampling);
          			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "start_capture - host:%s,port:%d, path:%s, sampling:%d\n",host,port, path, sampling);

          			status = start_capture(lsession, flags, host, port, path, sampling, sslFlags, "mod_aai_transcription");
        		}
			}
      		else 
			{
        		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "unsupported mod_aai_transcription cmd: %s\n", argv[1]);
      		}
			switch_core_session_rwunlock(lsession);
		}
		else 
		{
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error locating session %s\n", argv[0]);
		}
	}

	if (status == SWITCH_STATUS_SUCCESS) {
		stream->write_function(stream, "+OK Success\n");
	} else {
		stream->write_function(stream, "-ERR Operation Failed\n");
	}

  done:

	switch_safe_free(mycmd);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_MODULE_LOAD_FUNCTION(mod_aai_transcription_load)
{
	switch_api_interface_t *api_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_aai_transcription API loading..\n");

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	/* create/register custom event message types */
	if (switch_event_reserve_subclass(EVENT_TRANSCRIPTION) != SWITCH_STATUS_SUCCESS ||
    switch_event_reserve_subclass(EVENT_ERROR) != SWITCH_STATUS_SUCCESS ||
    switch_event_reserve_subclass(EVENT_DISCONNECT) != SWITCH_STATUS_SUCCESS) {

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register an event subclass for mod_aai_transcription API.\n");
		return SWITCH_STATUS_TERM;
	}

	SWITCH_ADD_API(api_interface, "uuid_aai_transcription", "aai_transcription API", aai_transcription_function, AAI_API_SYNTAX);
	switch_console_set_complete("add uuid_aai_transcription start wss-url metadata");
	switch_console_set_complete("add uuid_aai_transcription start wss-url");
	switch_console_set_complete("add uuid_aai_transcription stop");

	aai_init();

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_aai_transcription API successfully loaded\n");

	/* indicate that the module should continue to be loaded */
  //mod_running = 1;
	return SWITCH_STATUS_SUCCESS;
}

/*
  Called when the system shuts down
  Macro expands to: switch_status_t mod_aai_transcription_shutdown() */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_aai_transcription_shutdown)
{
	aai_cleanup();
  //mod_running = 0;
	switch_event_free_subclass(EVENT_TRANSCRIPTION);
	switch_event_free_subclass(EVENT_ERROR);

	return SWITCH_STATUS_SUCCESS;
}

/*
  If it exists, this is called in it's own thread when the module-load completes
  If it returns anything but SWITCH_STATUS_TERM it will be called again automatically
  Macro expands to: switch_status_t mod_aai_transcription_runtime()
*/
/*
SWITCH_MODULE_RUNTIME_FUNCTION(mod_aai_transcription_runtime)
{
  fork_service_threads(&mod_running);
	return SWITCH_STATUS_TERM;
}
*/
