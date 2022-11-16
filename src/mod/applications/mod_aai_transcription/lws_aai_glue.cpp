#include <switch.h>
#include <switch_json.h>
#include <string.h>
#include <string>
#include <mutex>
#include <thread>
#include <list>
#include <algorithm>
#include <functional>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <regex>

#include "base64.hpp"
#include "parser.hpp"
#include "mod_aai_transcription.h"
#include "audio_pipe.hpp"

// #define RTP_PACKETIZATION_PERIOD 20
// #define FRAME_SIZE_8000  320 /*which means each 20ms frame as 320 bytes at 8 khz (1 channel only)*/
// #define AAI_TRANSCRIPTION_FRAME_SIZE  FRAME_SIZE_8000  * 15 /*which means each 150ms*/

namespace {
  static const char *requestedBufferSecs = std::getenv("MOD_AUDIO_AAI_BUFFER_SECS");
  static const char *numberOfFramesForTranscription = std::getenv("MOD_AAI_TRANSCRIPTION_FRAME_SIZE");
  static int nAudioBufferSecs = std::max(1, std::min(requestedBufferSecs ? ::atoi(requestedBufferSecs) : 2, 5));
  static const char *requestedNumServiceThreads = std::getenv("MOD_AAI_TRANSCRIPTION_SERVICE_THREADS");
  static const char* mySubProtocolName = std::getenv("MOD_AAI_TRANSCRIPTION_SUBPROTOCOL_NAME") ?
    std::getenv("MOD_AAI_TRANSCRIPTION_SUBPROTOCOL_NAME") : "transcription.norwoodsystems.com";
  static unsigned int nServiceThreads = std::max(1, std::min(requestedNumServiceThreads ? ::atoi(requestedNumServiceThreads) : 1, 5));
  static unsigned int idxCallCount = 0;
  static uint32_t playCount = 0;
  static uint32_t base64AudioSize = norwood::b64_encoded_size(FRAME_SIZE_8000 * ::atoi(numberOfFramesForTranscription) * 2);
  // static char textToSend[(base64AudioSize  + 20) * 2];

  void processIncomingMessage(private_t* tech_pvt, switch_core_session_t* session, const char* message) {
    std::string msg = message;
    std::string type;
    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - received %s message\n", message);
    cJSON* json = parse_json(session, msg, type) ;
    if (json) {
      // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%u) processIncomingMessage - received %s message\n", tech_pvt->id, type.c_str());
      if (0 == type.compare("json")) {
        cJSON* jsonMsgType = cJSON_GetObjectItem(json, "message_type");
        // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - jsonMsgType:%s\n", jsonMsgType->valuestring);
        if (jsonMsgType && jsonMsgType->valuestring) {
          if (0 == strcmp(jsonMsgType->valuestring, "FinalTranscript")) 
          {
            cJSON* jsonTranscription = cJSON_GetObjectItem(json, "text");
            switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - FinalTranscript - text:%s\n", jsonTranscription->valuestring);
            if (jsonTranscription && jsonTranscription->valuestring) 
            {
                // char* jsonString = cJSON_PrintUnformatted(jsonTranscription);
                // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - FinalTranscript - transcripion:%s\n", jsonString);
                if ( strlen(jsonTranscription->valuestring) > 0) 
                {
                  AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
                  switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - FinalTranscript - text:%s, response_time:%ld [ms]\n", jsonTranscription->valuestring, ((switch_micro_time_now() - pAudioPipe->getSilenceStartTime())/1000));
                  tech_pvt->responseHandler(session, EVENT_TRANSCRIPTION, jsonTranscription->valuestring);
                }
                // free(jsonString);
            }
          } 
          else switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage -  message_type:%s\n",jsonMsgType->valuestring);
        } 
        // else switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "processIncomingMessage - message_type is not string\n");
      }
      else if (0 == type.compare("transcription")) {
        cJSON* jsonData = cJSON_GetObjectItem(json, "data");
        char* jsonString = cJSON_PrintUnformatted(jsonData);
        tech_pvt->responseHandler(session, EVENT_TRANSCRIPTION, jsonString);
        free(jsonString);        
      }
      else if (0 == type.compare("error")) {
        cJSON* jsonData = cJSON_GetObjectItem(json, "data");
        char* jsonString = cJSON_PrintUnformatted(jsonData);
        tech_pvt->responseHandler(session, EVENT_ERROR, jsonString);
        free(jsonString);        
      }
      else {
        switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "(%u) processIncomingMessage - unsupported msg type %s\n", tech_pvt->id, type.c_str());  
      }
      cJSON_Delete(json);
    }
    else {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%u) processIncomingMessage - could not parse message: %s\n", tech_pvt->id, message);
    }
  }

  static void eventCallback(const char* sessionId, AudioPipe::NotifyEvent_t event, const char* message) {
    switch_core_session_t* session = switch_core_session_locate(sessionId);
    if (session) {
      switch_channel_t *channel = switch_core_session_get_channel(session);
      switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
      if (bug) {
        private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
        if (tech_pvt) {
          switch (event) {
            case AudioPipe::CONNECT_SUCCESS:
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "connection successful - flush media_bug_buffer\n");
              // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "connection successful - DON'T flush media_bug_buffer\n");
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "connection successful - sessionId: %s message:%s\n", sessionId, message);
              tech_pvt->responseHandler(session, EVENT_CONNECT_SUCCESS, NULL);
              
              //We are connected and ready for transcription; let's flush audio buffer
              switch_core_media_bug_flush(bug);
              // AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
              // if(pAudioPipe) {
              //   pAudioPipe->audioWritePtrResetToZero();
              //   pAudioPipe->clearMetadata();
              // }

              // if (strlen(tech_pvt->initialMetadata) > 0) {
              //   switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "sending initial metadata %s\n", tech_pvt->initialMetadata);
              //   AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
              //   pAudioPipe->bufferForSending(tech_pvt->initialMetadata);
              // }
            break;
            case AudioPipe::CONNECT_FAIL:
            {
              // first thing: we can no longer access the AudioPipe
              std::stringstream json;
              json << "{\"reason\":\"" << message << "\"}";
              tech_pvt->pAudioPipe = nullptr;
              tech_pvt->responseHandler(session, EVENT_CONNECT_FAIL, (char *) json.str().c_str());
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "connection failed: %s\n", message);
            }
            break;
            case AudioPipe::CONNECTION_DROPPED:
              // first thing: we can no longer access the AudioPipe
              tech_pvt->pAudioPipe = nullptr;
              tech_pvt->responseHandler(session, EVENT_DISCONNECT, NULL);
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "connection dropped from far end\n");
            break;
            case AudioPipe::CONNECTION_CLOSED_GRACEFULLY:
              // first thing: we can no longer access the AudioPipe
              tech_pvt->pAudioPipe = nullptr;
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "connection closed gracefully\n");
            break;
            case AudioPipe::MESSAGE:
              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "AAI eventCallback - sessionId: %s message:%s\n", sessionId, message);
              processIncomingMessage(tech_pvt, session, message);
            break;
          }
        }
      }
      switch_core_session_rwunlock(session);
    }
  }
  switch_status_t aai_data_init(private_t *tech_pvt, switch_core_session_t *session, char * host, 
    unsigned int port, char* path, int sslFlags, int sampling, int desiredSampling, int channels, responseHandler_t responseHandler) {

    const char* api_token = nullptr;
    int err;
    switch_codec_implementation_t read_impl;
    switch_channel_t *channel = switch_core_session_get_channel(session);

    switch_core_session_get_read_impl(session, &read_impl);

    memset(tech_pvt, 0, sizeof(private_t));
  
    // VR- path - url encode here
    // char out[MAX_PATH_LEN] = "/v2/realtime/ws?sample_rate%3D16000%26word_boost%3D%5B%22Nedlands%22%2C%22TuartHill%22%5D";  // AAI suggestions
    // char out[MAX_PATH_LEN] = "/v2/realtime/ws%3F%22sample_rate%22%3A16000%2C%22word_boost%22%3A%5B%22Nedlands%22%2C%22TuartHill%22%5D";
    // char out[MAX_PATH_LEN] = "/v2/realtime/ws?sample_rate%3A16000%2Cword_boost%3A%5B%22Nedlands%22%2C%22TuartHill%22%5D";
    char out[MAX_PATH_LEN] = "/v2/realtime/ws?sample_rate%3D16000%2Cword_boost%3A%5B%22Nedlands%22%2C%22TuartHill%22%5D";
    // char in[MAX_PATH_LEN] = "\"sample_rate\":16000, \"word_boost\":[\"Nedlands\", \"Tuart Hill\"]";
    // char out1[MAX_PATH_LEN] = "";
    // char out[MAX_PATH_LEN] = "/v2/realtime/ws?";
    
    // switch_url_encode(in, out1, sizeof(out1));
    // strcat(out, out1);
    // char out[MAX_PATH_LEN] = "/v2/realtime/ws?%22sample_rate%22%3A16000%2C%20%22word_boost%22%3A%5B%22Nedlands%22%2C%20%22Tuart%20Hill%22%5D";
    // char out2[MAX_PATH_LEN] = "/v2/realtime/ws?\"sample_rate\":16000,\"word_boost\":[\"Nedlands\", \"Tuart Hill\"]";

    // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_data_init - url_encode - in: %s, out1:%s, out:%s, host:%s\n",path,out1,out, host);
    // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_data_init - path: %s\n",out2);
  
    strncpy(tech_pvt->sessionId, switch_core_session_get_uuid(session), MAX_SESSION_ID);
    strncpy(tech_pvt->host, host, MAX_WS_URL_LEN);
    tech_pvt->port = port;

    strncpy(tech_pvt->path, out, MAX_PATH_LEN);    
    tech_pvt->sampling = desiredSampling;
    tech_pvt->responseHandler = responseHandler;
    tech_pvt->playout = NULL;
    tech_pvt->channels = channels;
    tech_pvt->id = ++idxCallCount;
    tech_pvt->buffer_overrun_notified = 0;
    tech_pvt->audio_paused = 0;
    tech_pvt->graceful_shutdown = 0;
    
    size_t buflen = (FRAME_SIZE_8000 * desiredSampling / 8000 * channels * 1000 / RTP_PACKETIZATION_PERIOD * nAudioBufferSecs);

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_data_init -desiredSampling:%d,nAudioBufferSecs:%u decoded_bytes_per_packet:%u, buflen: %u \n",desiredSampling,nAudioBufferSecs,read_impl.decoded_bytes_per_packet, buflen);
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_data_init - ech_pvt->sampling:%d,tech_pvt->path: %s\n",tech_pvt->sampling,tech_pvt->path);

    AudioPipe* ap = new AudioPipe(tech_pvt->sessionId, host, port, out, sslFlags, 
      buflen, read_impl.decoded_bytes_per_packet, eventCallback);
    if (!ap) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error allocating AudioPipe\n");
      return SWITCH_STATUS_FALSE;
    }

    tech_pvt->pAudioPipe = static_cast<void *>(ap);

    switch_mutex_init(&tech_pvt->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

    if (desiredSampling != sampling) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "(%u) resampling from %u to %u\n", tech_pvt->id, sampling, desiredSampling);
      tech_pvt->resampler = speex_resampler_init(channels, sampling, desiredSampling, SWITCH_RESAMPLE_QUALITY, &err);
      if (0 != err) {
        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Error initializing resampler: %s.\n", speex_resampler_strerror(err));
        return SWITCH_STATUS_FALSE;
      }
    }
    else {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "(%u) no resampling needed for this call\n", tech_pvt->id);
    }

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "(%u) aai_data_init\n", tech_pvt->id);

    return SWITCH_STATUS_SUCCESS;
  }

  void destroy_tech_pvt(private_t* tech_pvt) {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "%s (%u) destroy_tech_pvt\n", tech_pvt->sessionId, tech_pvt->id);
    if (tech_pvt->resampler) {
      speex_resampler_destroy(tech_pvt->resampler);
      tech_pvt->resampler = nullptr;
    }
    if (tech_pvt->mutex) {
      switch_mutex_destroy(tech_pvt->mutex);
      tech_pvt->mutex = nullptr;
    }
  }

  void lws_logger(int level, const char *line) {
    switch_log_level_t llevel = SWITCH_LOG_DEBUG;

    switch (level) {
      case LLL_ERR: llevel = SWITCH_LOG_ERROR; break;
      case LLL_WARN: llevel = SWITCH_LOG_WARNING; break;
      case LLL_NOTICE: llevel = SWITCH_LOG_NOTICE; break;
      case LLL_INFO: llevel = SWITCH_LOG_INFO; break;
      break;
    }
	  switch_log_printf(SWITCH_CHANNEL_LOG, llevel, "%s\n", line);
  }
}

extern "C" {
  int parse_ws_uri(switch_channel_t *channel, const char* szServerUri, char* host, char *path, unsigned int* pPort, int* pSslFlags) {
    int i = 0, offset;
    char server[MAX_WS_URL_LEN + MAX_PATH_LEN];
    char *saveptr;
    int flags = LCCSCF_USE_SSL;
    
    if (switch_true(switch_channel_get_variable(channel, "MOD_AUDIO_FORK_ALLOW_SELFSIGNED"))) {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "parse_ws_uri - allowing self-signed certs\n");
      flags |= LCCSCF_ALLOW_SELFSIGNED;
    }
    if (switch_true(switch_channel_get_variable(channel, "MOD_AUDIO_FORK_SKIP_SERVER_CERT_HOSTNAME_CHECK"))) {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "parse_ws_uri - skipping hostname check\n");
      flags |= LCCSCF_SKIP_SERVER_CERT_HOSTNAME_CHECK;
    }
    if (switch_true(switch_channel_get_variable(channel, "MOD_AUDIO_FORK_ALLOW_EXPIRED"))) {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "parse_ws_uri - allowing expired certs\n");
      flags |= LCCSCF_ALLOW_EXPIRED;
    }

    // get the scheme
    strncpy(server, szServerUri, MAX_WS_URL_LEN + MAX_PATH_LEN);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "server: %s\n", server);

    if (0 == strncmp(server, "https://", 8) || 0 == strncmp(server, "HTTPS://", 8)) {
      *pSslFlags = flags;
      offset = 8;
      *pPort = 443;
    }
    else if (0 == strncmp(server, "wss://", 6) || 0 == strncmp(server, "WSS://", 6)) {
      *pSslFlags = flags;
      offset = 6;
      *pPort = 443;
    }
    else if (0 == strncmp(server, "http://", 7) || 0 == strncmp(server, "HTTP://", 7)) {
      offset = 7;
      *pSslFlags = 0;
      *pPort = 80;
    }
    else if (0 == strncmp(server, "ws://", 5) || 0 == strncmp(server, "WS://", 5)) {
      offset = 5;
      *pSslFlags = 0;
      *pPort = 80;
    }
    else {
      switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "parse_ws_uri - error parsing uri %s: invalid scheme\n", szServerUri);;
      return 0;
    }

    std::string strHost;
    strHost.reserve(MAX_PATH_LEN);
    strncpy((char*)strHost.c_str(), (char*)(server+offset),MAX_PATH_LEN);

    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "strHost: %s\n", strHost.c_str());
    // switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "server+offset: %s\n", (server+offset));
    std::regex re("^(.+?):?(\\d+)?(/.*)?$");
    std::smatch matches;
    // if(std::regex_search(strHost, matches, re)) {
    //   for (int i = 0; i < matches.length(); i++) {
    //     switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "parse_ws_uri - %d: %s\n", i, matches[i].str().c_str());
    //   }
    //   strncpy(host, matches[1].str().c_str(), MAX_WS_URL_LEN);
    //   if (matches[2].str().length() > 0) {
    //     *pPort = atoi(matches[2].str().c_str());
    //   }
    //   if (matches[3].str().length() > 0) {
    //     strncpy(path, matches[3].str().c_str(), MAX_PATH_LEN);
    //   }
    //   else {
    //     strcpy(path, "/");
    //   }
    // } else {
    //   switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "parse_ws_uri - invalid format %s\n", strHost.c_str());
    //   return 0;
    // }
/*    
    sample_rate = 16000
    word_boost = ["foo", "bar"]
    params = {"sample_rate": sample_rate, "word_boost": json.dumps(word_boost)}
    
    url = f"wss://api.assemblyai.com/v2/realtime/ws?{urlencode(params)}"
*/  
    // char path_vr[256] = "/v2/realtime/ws?{%7B%22sample_rate%22%3A8000%2C%22word_boost%22%3A%5B%22Nedlands%22%2C%22Tuart+Hill%22%2C%22North+Perth%22%2C%22Como%22%5D%7D}";
    // char host_vr[20] = "api.assemblyai.com";
    
    char path_vr[256] = "/v2/realtime/ws?\"sample_rate\":8000,\"word_boost\":[\"Nedlands\",\"TuartHill\"]";
    char host_vr[20] = "api.assemblyai.com";

    strncpy(path, path_vr, MAX_PATH_LEN);
    strncpy(host, host_vr, MAX_WS_URL_LEN);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "parse_ws_uri - host: %s, path: %s\n", host, path);

    return 1;
  }

  switch_status_t aai_init() {
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_aai_transcription: audio buffer (in secs):    %d secs\n", nAudioBufferSecs);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_aai_transcription: sub-protocol:              %s\n", mySubProtocolName);
    switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "mod_aai_transcription: lws service threads:       %d\n", nServiceThreads);
 
    int logs = LLL_ERR | LLL_WARN | LLL_NOTICE ;
     //LLL_INFO | LLL_PARSER | LLL_HEADER | LLL_EXT | LLL_CLIENT  | LLL_LATENCY | LLL_DEBUG ;
    AudioPipe::initialize(mySubProtocolName, nServiceThreads, logs, lws_logger);
   return SWITCH_STATUS_SUCCESS;
  }

  switch_status_t aai_cleanup() {
    bool cleanup = false;
    cleanup = AudioPipe::deinitialize();
    if (cleanup == true) {
        return SWITCH_STATUS_SUCCESS;
    }
    return SWITCH_STATUS_FALSE;
  }

  switch_status_t aai_session_init(switch_core_session_t *session, 
              responseHandler_t responseHandler,
              uint32_t samples_per_second, 
              char *host,
              unsigned int port,
              char *path,
              int sampling,
              int sslFlags,
              int channels,
              void **ppUserData)
  {    	
    int err;

    // allocate per-session data structure
    private_t* tech_pvt = (private_t *) switch_core_session_alloc(session, sizeof(private_t));
    if (!tech_pvt) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "error allocating memory!\n");
      return SWITCH_STATUS_FALSE;
    }

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_session_init - samples_per_second:%u,sampling:%d,channels:%d \n", samples_per_second,sampling, channels);

    if (SWITCH_STATUS_SUCCESS != aai_data_init(tech_pvt, session, host, port, path, sslFlags, samples_per_second, sampling, channels, responseHandler)) {
      destroy_tech_pvt(tech_pvt);
      return SWITCH_STATUS_FALSE;
    }

    *ppUserData = tech_pvt;

    AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
    pAudioPipe->connect();
    return SWITCH_STATUS_SUCCESS;
  }

  switch_status_t aai_session_cleanup(switch_core_session_t *session, char* text, int channelIsClosing) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
    if (!bug) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "aai_session_cleanup: no bug - websocket conection already closed\n");
      return SWITCH_STATUS_FALSE;
    }
    private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
    uint32_t id = tech_pvt->id;

    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "(%u) aai_session_cleanup\n", id);

    if (!tech_pvt) return SWITCH_STATUS_FALSE;
    AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
      
    switch_mutex_lock(tech_pvt->mutex);

    // get the bug again, now that we are under lock
    {
      switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
      if (bug) {
        switch_channel_set_private(channel, MY_BUG_NAME, NULL);
        if (!channelIsClosing) {
          switch_core_media_bug_remove(session, &bug);
        }
      }
    }

    // delete any temp files
    struct playout* playout = tech_pvt->playout;
    while (playout) {
      std::remove(playout->file);
      free(playout->file);
      struct playout *tmp = playout;
      playout = playout->next;
      free(tmp);
    }

    if (pAudioPipe && text) pAudioPipe->bufferForSending(text, strlen(text));
    if (pAudioPipe) pAudioPipe->close();

    destroy_tech_pvt(tech_pvt);
    switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "(%u) aai_session_cleanup: connection closed\n", id);
    return SWITCH_STATUS_SUCCESS;
  }

  switch_status_t aai_session_send_text(switch_core_session_t *session, char* text) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
    if (!bug) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "aai_session_send_text failed because no bug\n");
      return SWITCH_STATUS_FALSE;
    }
    private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
  
    if (!tech_pvt) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "aai_session_send_text failed because no tech_pvt\n");
      return SWITCH_STATUS_FALSE;
    }
    AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
    if (pAudioPipe && text) pAudioPipe->bufferForSending(text, strlen(text));

    return SWITCH_STATUS_SUCCESS;
  }

  switch_status_t aai_session_pauseresume(switch_core_session_t *session, int pause) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
    if (!bug) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "aai_session_pauseresume failed because no bug\n");
      return SWITCH_STATUS_FALSE;
    }
    private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
  
    if (!tech_pvt) return SWITCH_STATUS_FALSE;

    switch_core_media_bug_flush(bug);
    tech_pvt->audio_paused = pause;
    return SWITCH_STATUS_SUCCESS;
  }

  switch_status_t aai_session_graceful_shutdown(switch_core_session_t *session) {
    switch_channel_t *channel = switch_core_session_get_channel(session);
    switch_media_bug_t *bug = (switch_media_bug_t*) switch_channel_get_private(channel, MY_BUG_NAME);
    if (!bug) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "aai_session_graceful_shutdown failed because no bug\n");
      return SWITCH_STATUS_FALSE;
    }
    private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
  
    if (!tech_pvt) return SWITCH_STATUS_FALSE;

    tech_pvt->graceful_shutdown = 1;

    AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
    if (pAudioPipe) pAudioPipe->do_graceful_shutdown();

    return SWITCH_STATUS_SUCCESS;
  }

  switch_bool_t aai_frame(switch_core_session_t *session, switch_media_bug_t *bug) {
    private_t* tech_pvt = (private_t*) switch_core_media_bug_get_user_data(bug);
    size_t inuse = 0;
    bool dirty = false;

    if (!tech_pvt || tech_pvt->audio_paused || tech_pvt->graceful_shutdown) {
      switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - return - audio paused or shutdown\n");
      return SWITCH_TRUE;
    }
    if (switch_mutex_trylock(tech_pvt->mutex) == SWITCH_STATUS_SUCCESS) {
      if (!tech_pvt->pAudioPipe) {
        switch_mutex_unlock(tech_pvt->mutex);
        // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - return - no pAudioPipe\n");
        return SWITCH_TRUE;
      }
      AudioPipe *pAudioPipe = static_cast<AudioPipe *>(tech_pvt->pAudioPipe);
      if (pAudioPipe->getLwsState() != AudioPipe::LWS_CLIENT_CONNECTED) {
        // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - return - pAudioPipe is not CONNECTED\n");
        switch_mutex_unlock(tech_pvt->mutex);
        return SWITCH_TRUE;
      }

      pAudioPipe->lockAudioBuffer();
      size_t available = pAudioPipe->audioSpaceAvailable();
      if (NULL == tech_pvt->resampler) {
        switch_frame_t frame = { 0 };
        frame.data = pAudioPipe->audioWritePtr();
        frame.buflen = available;
        size_t transcription_size = FRAME_SIZE_8000 * ::atoi(numberOfFramesForTranscription);

        while (true) {

          // check if buffer would be overwritten; dump packets if so
          if (available < pAudioPipe->audioMinSpace()) {
            if (!tech_pvt->buffer_overrun_notified) {
              tech_pvt->buffer_overrun_notified = 1;
              tech_pvt->responseHandler(session, EVENT_BUFFER_OVERRUN, NULL);
            }
            switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "(%u) dropping packets!\n", 
              tech_pvt->id);
            pAudioPipe->audioWritePtrResetToZero();

            frame.data = pAudioPipe->audioWritePtr();
            frame.buflen = available = pAudioPipe->audioSpaceAvailable();
          }

          switch_status_t rv = switch_core_media_bug_read(bug, &frame, SWITCH_TRUE);
          if (rv != SWITCH_STATUS_SUCCESS) break;
          if (frame.datalen) {
            // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "8000 samling rate - READING frame.datalen: %u\n",frame.datalen); 
            pAudioPipe->audioWritePtrAdd(frame.datalen);
            frame.buflen = available = pAudioPipe->audioSpaceAvailable();
            frame.data = pAudioPipe->audioWritePtr();
            dirty = true;
            if (pAudioPipe->audioSpaceSize() >= transcription_size) {

         			int16_t *fdata = (int16_t *) pAudioPipe->audioReadPtr();
              uint32_t samples = transcription_size / sizeof(*fdata);
              uint32_t score, count = 0, j = 0;
              double energy = 0;


              for (count = 0; count < samples ; count++) {
                energy += abs(fdata[j++]);
              }

              score = (uint32_t) (energy / samples);

              switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "TRANSCRIPTION FRAME ENERGY:%d, samples:%d\n",score, samples);

         			// uint8_t *fdata1 = (uint8_t *) pAudioPipe->audioReadPtr();
              // uint32_t samples1 =  transcription_size/ sizeof(*fdata1);
              // uint32_t score1, count1 = 0, j1 = 0;
              // double energy1 = 0;
              // for (count1 = 0; count1 < samples1; count1++) {
              //   energy1 += abs(fdata1[j1++]);
              // }
              // score1 = (uint32_t) (energy1 / samples1);
              // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "TRANSCRIPTION FRAME ENERGY1:%d, samples1:%d\n",score1, samples1);

              if (score > 100) {
                if (pAudioPipe->isAudioDetected() == false) {
                  pAudioPipe->audioDetected(true);
                  pAudioPipe->silenceDetected(false);
                }
              } else if (pAudioPipe->isAudioDetected() == true) {
                pAudioPipe->audioDetected(false);
                pAudioPipe->silenceDetected(true);
                pAudioPipe->storeSilenceStartTime(switch_micro_time_now());
              }


              char textToSend[(base64AudioSize/2  + 20)];
              memset(textToSend, '\0', sizeof(textToSend));
              strcat(textToSend, "{\"audio_data\":\"");
              // strcat(textToSend, pAudioPipe->base64EncodedAudio(transcription_size).c_str());
              strcat(textToSend, pAudioPipe->b64AudioEncoding(transcription_size));
              strcat(textToSend, "\"}");
              // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - base64_encode audio - textToSend:%s, len:%u\n", textToSend, strlen(textToSend));
              pAudioPipe->audioWritePtrSubtract(transcription_size);

              pAudioPipe->bufferForSending(textToSend, strlen(textToSend));
              // aai_session_send_text(session, textToSend);
              // aai_session_send_text(session, (char*)json.str());
              break; 
            }
            else {
              // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Not enough data to send - audioSpaceSize: %u\n", pAudioPipe->audioSpaceSize());
            }

          }
        }
      }
      else {
        uint8_t data[FRAME_SIZE_8000];
        // uint8_t data[AAI_TRANSCRIPTION_FRAME_SIZE]; // 100 msec of audio
        switch_frame_t frame = { 0 };
        frame.data = data;
        frame.buflen = FRAME_SIZE_8000;
        // frame.buflen = AAI_TRANSCRIPTION_FRAME_SIZE;

        size_t transcription_size = FRAME_SIZE_8000 * ::atoi(numberOfFramesForTranscription) * 2;
        // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - resampler != null");

        while (switch_core_media_bug_read(bug, &frame, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS ) {
          if (frame.datalen) {
            spx_uint32_t out_len = available >> 1;  // space for samples which are 2 bytes
            spx_uint32_t in_len = frame.samples;

            speex_resampler_process_interleaved_int(tech_pvt->resampler, 
              (const spx_int16_t *) frame.data, 
              (spx_uint32_t *) &in_len, 
              (spx_int16_t *) ((char *) pAudioPipe->audioWritePtr()),
              &out_len);
            // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - datalen:%u, in_len: %u, new value out_len:%u channels:%u\n",frame.datalen,in_len, out_len, tech_pvt->channels);

            if (out_len > 0) {
              // bytes written = num channels * 2 * num channels
              size_t bytes_written = out_len << tech_pvt->channels;
              pAudioPipe->audioWritePtrAdd(bytes_written);
              available = pAudioPipe->audioSpaceAvailable();
              dirty = true;
              // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - bytes_written:%u, available:%u, audioBufferSize: %u\n", bytes_written, available, pAudioPipe->audioSpaceSize());

              if (pAudioPipe->audioSpaceSize() >= transcription_size) {

                char textToSend[(base64AudioSize  + 20)];
                memset(textToSend, '\0', sizeof(textToSend));
                strcat(textToSend, "{\"audio_data\":\"");
                // strcat(textToSend, pAudioPipe->base64EncodedAudio(transcription_size).c_str());
                strcat(textToSend, pAudioPipe->b64AudioEncoding(transcription_size));
                strcat(textToSend, "\"}");
                // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "aai_frame - base64_encode audio - textToSend:%s, len:%u\n", textToSend, strlen(textToSend));
                pAudioPipe->audioWritePtrSubtract(transcription_size);

                pAudioPipe->bufferForSending(textToSend, strlen(textToSend));
                // aai_session_send_text(session, textToSend);
                // aai_session_send_text(session, (char*)json.str());
                break; 
              }
              else {
                // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Not enough data to send - audioSpaceSize: %u\n", pAudioPipe->audioSpaceSize());
              }

            }
            if (available < pAudioPipe->audioMinSpace()) {
              if (!tech_pvt->buffer_overrun_notified) {
                tech_pvt->buffer_overrun_notified = 1;
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "(%u) dropping packets!\n", 
                  tech_pvt->id);
                tech_pvt->responseHandler(session, EVENT_BUFFER_OVERRUN, NULL);
              }
              break;
            }
          }
        }
      }

      pAudioPipe->unlockAudioBuffer();
      switch_mutex_unlock(tech_pvt->mutex);
    }
    // switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "EXIT aai_frame \n");

    return SWITCH_TRUE;
  }

}

