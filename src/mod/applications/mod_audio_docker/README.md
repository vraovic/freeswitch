# mod_audio_docker

A Freeswitch module that attaches a bug to a media server endpoint and streams L16 audio via websockets to a remote server.  This module also supports receiving media from the server to play back to the caller, enabling the creation of full-fledged IVR or dialog-type applications.

## API

### Commands
The freeswitch module exposes the following API commands:

```
uuid_audio_docker <uuid> start <wss-url> <metadata>
```
Attaches media bug and starts streaming audio stream to the back-end server.  Audio is streamed in linear 16 format (16-bit PCM encoding) with either one or two channels depending on the mix-type requested.
- `uuid` - unique identifier of Freeswitch channel
- `wss-url` - websocket url to connect and stream audio to
- `sampling-rate` - choice of
  - "8k" = 8000 Hz sample rate will be generated
  - "16k" = 16000 Hz sample rate will be generated
- `metadata` - a text frame of arbitrary data to send to the back-end server immediately upon connecting.  Once this text frame has been sent, the incoming audio will be sent in binary frames to the server.

```
uuid_audio_docker <uuid> send_text <metadata>
```
Send a text frame of arbitrary data to the remote server (e.g. this can be used to notify of DTMF events).

```
uuid_audio_docker <uuid> stop <metadata>
```
Closes websocket connection and detaches media bug, optionally sending a final text frame over the websocket connection before closing.

### Events
An optional feature of this module is that it can receive JSON text frames from the server and generate associated events to an application.  The format of the JSON text frames and the associated events are described below.

