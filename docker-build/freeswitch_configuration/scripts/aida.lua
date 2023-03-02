-- Answer call, play a prompt, hang up 
-- Set the path separator 
JSON = assert(loadfile "/usr/local/freeswitch/scripts/json.lua")() -- one-time load of the routines
JSON.strictTypes = true
function encodeparams(paramsT)
    function escape (s)
      s = string.gsub(
            s, 
            '([\r\n"#%%&+:;<=>?@^`{|}%\\%[%]%(%)$!~,/\'])', 
            function (c)
                    return '%'..string.format("%X", string.byte(c))
            end
    )
      s = string.gsub(s, "%s", "+")
      return s
    end

    function encode (t)
      local s = ""
      for k , v in pairs(t) do
            s = s .. "&" .. escape(k) .. "=" .. escape(v)
      end
      return string.sub(s, 2)     -- remove first `&'
    end
    
    if type(paramsT) == 'table' then
            return encode(paramsT)
    else
            local tmp = Utils:commaText(paramsT, '&'); 
            local myParamsT = {};
            for k, v in pairs(tmp) do
                    local pos = 0
                    pos = string.find(v, '=')
                    if not pos then return '' end
                    myParamsT[string.sub(v, 1, pos-1 )] = string.sub(v, pos+1 )
            end
            return encode(myParamsT)
    end
end

local char_to_hex = function(c)
    return string.format("%%%02X", string.byte(c))
  end
  
  local function urlencode(url)
    if url == nil then
      return
    end
    url = url:gsub("\n", "\r\n")
    url = url:gsub("([^%w ])", char_to_hex)
    url = url:gsub(" ", "+")
    return url
  end
  
  local hex_to_char = function(x)
    return string.char(tonumber(x, 16))
  end

--
-- JSON = (loadfile(script_root .. "json.lua"))()
pathsep = '/' 
-- Windows users do this instead: 
-- pathsep = '\' 
freeswitch.consoleLog("debug","before creating the API object\n")   
api = freeswitch.API()   
freeswitch.consoleLog("debug","after creating the API object\n")   
reply = api:executeString("version") 
freeswitch.consoleLog("debug","reply is: " .. reply .."\n") 
reply = api:executeString("status") 
freeswitch.consoleLog("debug","reply is: " .. reply .."\n") 
-- reply = api:executeString("sofia status") 
-- freeswitch.consoleLog("WARNING","reply is: " .. reply .."\n") 

-- Answer the call 
freeswitch.consoleLog("debug","Before answer\n") 
session:answer() 
freeswitch.consoleLog("debug","After answer\n") 

uuid = session:getVariable("uuid") 
freeswitch.consoleLog("debug","Inbound UUID: " .. uuid .."\n") 
uuid1 = session:get_uuid()
-- freeswitch.consoleLog("WARNING","Inbound UUID(get_uuid): " .. uuid1 .."\n") 

session_token = ""
aida_greeting = ""
aida_server = session:getVariable('aida_server')

-- Try to bridge call
-- second_session = freeswitch.Session("sofia/internal/+6421234569@192.168.44.5")  
-- if (second_session:ready()) then
--     freeswitch.consoleLog("WARNING","second leg answered\n")  
--     freeswitch.bridge(session, second_session)
--     freeswitch.consoleLog("WARNING","After bridge\n")  
-- end

-- ==============================================


-- Get called number from passed argument to lua script
called_number = argv[1];

freeswitch.consoleLog("debug","AIDA_Server:" .. aida_server .. " called number: " .. called_number .. "\n") 
destination_number = session:getVariable("destination_number");
caller_id_name = session:getVariable("caller_id_name");
caller_id_number = session:getVariable("caller_id_number");
freeswitch.consoleLog("debug","destination_number:" .. destination_number .. " caller_id_name:" .. caller_id_name .. " caller_id_number:" .. caller_id_number .. "\n") 

-- reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " start wss://api.assemblyai.com/v2/realtime/ws?sample_rate=8000") 

-- Custom vocabulary

-- python example
-- sample_rate = 16000
-- word_boost = ["foo", "bar"]
-- params = {"sample_rate": sample_rate, "word_boost": json.dumps(word_boost)}
-- url = f"wss://api.assemblyai.com/v2/realtime/ws?{urlencode(params)}"

local suburbs_list = '['
local point_file = io.open("/usr/local/freeswitch/scripts/suburbs.txt", "r")
for line in point_file:lines() do
    freeswitch.consoleLog("WARNING", line .. "\n")
    suburbs_list = suburbs_list .. '"' ..  line .. '",' 
    --  table.insert(line_data, player_data)
end
suburbs_list = suburbs_list:sub(1, -2)
suburbs_list = suburbs_list .. ']'
freeswitch.consoleLog("WARNING","suburbs_list:" .. suburbs_list .. "\n") 
-- json_list = JSON:encode(suburbs_list)
-- suburbs_list_decoded = JSON:decode(json_list)
-- freeswitch.consoleLog("WARNING","suburbs_list_decoded:" .. suburbs_list_decoded .. "\n") 

urlencoded_suburb_list = urlencode(suburbs_list)
freeswitch.consoleLog("WARNING","urlencoded_suburb_list:" .. urlencoded_suburb_list .. ",length:" .. #urlencoded_suburb_list .. "\n") 

-- url = "wss://api.assemblyai.com/v2/realtime/ws?sample_rate=8000"
url = "wss://api.assemblyai.com/v2/realtime/ws?sample_rate=8000&word_boost=" .. urlencoded_suburb_list
freeswitch.consoleLog("WARNING","URL:" .. url .. "\n") 
reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " start " .. url) 


freeswitch.consoleLog("WARNING","uuid_aai_transcription START - reply: " .. reply .."\n") 
aai_transcription = freeswitch.EventConsumer("CUSTOM","mod_aai_transcription::transcription")

-- We need to set zombie exec flag to execute curl command while the channel is hanging up
-- session:execute("set_zombie_exec")
-- Set TTS params
-- session:set_tts_params("google_tts", "en-GB-Wavenet-A");
voice = "en-GB-Wavenet-F"
session:set_tts_params("google_tts", voice);

if (session_token == "") then 
    url = aida_server .. "/sessions/create/" .. caller_id_number .. "/" .. called_number .. "/" .. uuid
    freeswitch.consoleLog("info","SESSIONS CREATE:" .. url .. "\n") 
    -- /createSession
    session:execute("curl", url)
    curl_response_code = session:getVariable("curl_response_code")
    curl_response      = session:getVariable("curl_response_data")
    if (curl_response_code == "200") then
        freeswitch.consoleLog("info","curl_response_code" .. curl_response_code .. "\n") 
        freeswitch.consoleLog("info","curl_response:" .. curl_response .. "\n") 
        -- Extract token from response
        a,b = curl_response:match("(.+)&(.+)")
        freeswitch.consoleLog("debug","curl_response -a:" .. a .. " curl_response -b:" .. b .. "\n") 
        aida_greeting = b
        c,d = a:match("(.+)&(.+)")
        freeswitch.consoleLog("debug","curl_response -c:" .. c .. " curl_response -b:" .. d .. "\n") 
        voice = d
        e,f = c:match("(.+)&(.+)")
        session_token = f:gsub("%s+", "")
        freeswitch.consoleLog("debug","TOKEN:" .. session_token .. " greeting:" .. aida_greeting .. " voice:" .. voice .. "\n") 
    else
        freeswitch.consoleLog("WARNING","NO RESPONSE for createSession\n") 
        if curl_response_code then
            freeswitch.consoleLog("WARNING","curl_response_code: " .. curl_response_code .. "\n") 
        end
        if curl_response then
            freeswitch.consoleLog("WARNING","curl_response: " .. curl_response .. "\n") 
        end
        session:sleep(700);
        session:speak("It appears, the user is not registered. Please check with your administrator. Thank you!");
        session:sleep(300);
        session:hangup()
    end 
end

-- set termination_reason variable: 0 - transfer Ok ; 1 - the system hangup;  2 - caller hangup
session:setVariable("termination_reason", "2") 
session:setVariable("hangup_in_progress", "0") 

session:sleep(100);
session:set_tts_params("google_tts", voice);
session:setVariable("playing_prompt", "1")
session:speak(aida_greeting);
-- session:speak("Hi! My name is Aïda and I'm Richard's virtual AI assistant. How can I help you today?");
session:sleep(100);
session:setVariable("playing_prompt", "0")


-- -- TODO: do we need async event
-- -- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US interim") 
-- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US") 
-- freeswitch.consoleLog("WARNING","uuid_google_transcribe START - reply: " .. reply .."\n") 

-- -- Register for google_transcribe event
-- transcription = freeswitch.EventConsumer("CUSTOM","google_transcribe::transcription")
-- no_audio = freeswitch.EventConsumer("CUSTOM","google_transcribe::no_audio_detected")
-- max_duration = freeswitch.EventConsumer("CUSTOM","google_transcribe::max_duration_exceeded")

-- Now get audio 
-- filename = session:getVariable('sounds_dir') .. "/" .. uuid .. ".wav"
-- freeswitch.consoleLog("debug","Get audio params - just BEFORE streaming\n") 
-- -- reply = api:executeString("bgapi uuid_record " .. uuid .. " start " .. filename)
-- -- state2 = session:getVariable("state");
-- -- freeswitch.consoleLog("debug", "STATE = " .. state2 .. "\n");
-- readcodec = session:getVariable("read_codec");
-- freeswitch.consoleLog("debug", "read(source) codec = " .. readcodec .. "\n");
-- soureceRate = session:getVariable("read_rate");
-- freeswitch.consoleLog("debug", "read(source) codec rate = " .. soureceRate .. "\n");
-- state2 = session:getState();
-- freeswitch.consoleLog("debug", "Channel-Call-State = " .. state2 .. "\n");
-- state3 = session:getVariable("Channel-State");
-- freeswitch.consoleLog("debug", "Channel-State = " .. state3 .. "\n");
-- session:sleep(700)
-- recordFile in a background thread; so call luarun 
session:setVariable("from_main_thread", "0")
api:execute("luarun", "bgAida.lua " .. uuid )
session:sleep(20)

-- Let's try to call google_transcribe after we run recording
-- TODO: do we need async event
-- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US interim") 


-- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US") 
-- freeswitch.consoleLog("WARNING","uuid_google_transcribe START - reply: " .. reply .."\n") 
-- reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " start wss://api.assemblyai.com/v2/realtime/ws?sample_rate=16000") 
-- reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " start wss://api.assemblyai.com/v2/realtime/ws?sample_rate=8000") 
-- freeswitch.consoleLog("WARNING","uuid_aai_transcription START - reply: " .. reply .."\n") 
-- -- Register for google_transcribe event
-- -- transcription = freeswitch.EventConsumer("CUSTOM","google_transcribe::transcription")
-- -- no_audio = freeswitch.EventConsumer("CUSTOM","google_transcribe::no_audio_detected")
-- -- max_duration = freeswitch.EventConsumer("CUSTOM","google_transcribe::max_duration_exceeded")
-- aai_transcription = freeswitch.EventConsumer("CUSTOM","mod_aai_transcription::transcription")

-- session:readAndStream(filename,2000,100,15)

while (session:ready() == true) do
    session:setAutoHangup(false);

    if (session:getVariable("termination_reason") == "1") then
        break 
    end

    -- filename = session:getVariable('sounds_dir') .. "/" .. uuid .. ".wav"
    
    -- -- Create a session with Aida 


    -- -- -- Register for google_transcribe event
    -- -- transcription = freeswitch.EventConsumer("CUSTOM","google_transcribe::transcription")
    -- -- -- TODO: do we need async event
    -- -- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US interim") 
    -- -- -- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " start en-US") 
    -- -- freeswitch.consoleLog("WARNING","uuid_google_transcribe START - reply: " .. reply .."\n") 
    -- -- Get audio 
    -- freeswitch.consoleLog("WARNING","Get audio - recordFile\n") 
    -- --                      20  - max_len - maximum length of the recording in seconds
    -- --                      100 - silence_threshold - energy level audio must fall below to be considered silence (500 is a good starting point).
    -- --                      2   - silence_secs - seconds of silence to interrupt the record.
    -- session:recordFile(filename,2000,100,3)
    -- Get transcription
    -- received_event = transcription:pop(1, 5000) 
    -- received_aai_event = aai_transcription:pop(1, 5000) 
    -- received_event = transcription:pop() 
    received_event = nil
    received_aai_event = aai_transcription:pop(1,5000) 
    if(received_aai_event and received_aai_event:getHeader("Channel-Call-UUID") == uuid) then 
        text = received_aai_event:getBody() 
        -- text = ""; length of empty text string is 2; 
        if (text ~= nil and (#text) > 4) then 

            -- wait if currently playing a message to an user
            while (session:getVariable("playing_prompt") == "1") do
                session:sleep(20)
            end
            session:setVariable("playing_prompt", "1")
            session:setVariable("session_updating", "1")
            -- Got a transcript! Let's play a beep tone to a caller;
            local beepFile = "/usr/local/freeswitch/sounds/Blip.wav"
            session:streamFile(beepFile)
            session:sleep(20)
            session:setVariable("playing_prompt", "0")

            -- Send text to Aida
            freeswitch.consoleLog("info","Text for Aida-Docker:" ..  text .. "text-length:" .. #text .. " UUID:" .. received_aai_event:getHeader("Channel-Call-UUID") .. "\n") 

            -- Send updateSession with new text string 
            text = text:gsub( "%\"", "" )
            text = text:gsub( " ", "%%20" )
            url1 = aida_server .. "/sessions/update/" .. session_token .. "/" .. text
            freeswitch.consoleLog("info","SESSION UPDATE:" .. url1 .. "\n") 
            -- /updateSession
            session:execute("curl", url1)
            curl_response_code = session:getVariable("curl_response_code")
            curl_response      = session:getVariable("curl_response_data")
            session:setVariable("session_updating", "0")
            if (curl_response_code == "200") then
                -- We need to check how many params response has:
                -- 2 params - play message and wait for a caller input
                -- 3 params - transfer call - play a messge and transfer the call to a number (3rd params)
                -- en-AU-Wavenet-C&Ok.I found him, he can talk to you&+61234567
                -- session:execute("transfer", "3000 XML default")  -- we need to exit the session after call transfer; won't be exited automatically
                freeswitch.consoleLog("info","Aida response:" .. curl_response ..  " UUID:" .. uuid .. "\n") 
                
                wavenet, aidaResponse, numberToTransfer = curl_response:match("(.+)&(.+)&?(.*)")
                if aidaResponse == nil or aidaResponse == "" then
                    aidaResponse = "Please wait a moment..."
                end
                freeswitch.consoleLog("info","wavenet:" .. wavenet .. " aidaResponse:" .. aidaResponse .. "\n") 
                session:setAutoHangup(false);
                session:set_tts_params("google_tts", wavenet);
                -- session:set_tts_params("google_tts", "en-GB-Wavenet-F");
                -- wiat if we already playing a prompt
                
                if (session:getVariable("hangup_in_progress") == "0") then 
                    while (session:getVariable("playing_prompt") == "1") do
                        session:sleep(20)
                        freeswitch.consoleLog("info","sessions_update - WAIT for previous prompt to finish.\n") 
                    end
                    session:setVariable("playing_prompt", "1")
                    session:sleep(70);
                    session:speak(aidaResponse);
                    session:sleep(200)
                    session:setVariable("playing_prompt", "0")
                    -- another_event = aai_transcription:pop(1,100)
                    -- if(another_event and another_event:getHeader("Channel-Call-UUID") == uuid) then 
                    --     text = another_event:getBody() 
                    --     freeswitch.consoleLog("debug","sessions_update - THROW AWAY - this should be a part of the previous dalog:" .. text .. "\n") 
                    -- else 
                    --     freeswitch.consoleLog("debug","sessions_update - NOTHING to throw!\n") 
                    -- end
                end
            else
                freeswitch.consoleLog("ERROR","CURL response_code:" .. curl_response_code .. " UUID:" .. uuid .. "\n") 
                if curl_response then 
                    freeswitch.consoleLog("ERROR","CURL response_data:" .. curl_response .. " UUID:" .. uuid .. "\n") 
                    -- Extract a message from curl_response
                    a,b = curl_response:match("(.+),(.+)")
                    freeswitch.consoleLog("WARNING","curl_response -a:" .. a .. " curl_response -b:" .. b .. "\n") 
                    c = ""
                    d = ""
                    if string.match(a, "error") then
                        c,d = b:match("(.+):(.+)")
                    else
                        c,d = a:match("(.+):(.+)")
                    end
                    freeswitch.consoleLog("WARNING","curl_response -c:" .. c .. " curl_response -b:" .. d .. "\n") 
                    session:setVariable("playing_prompt", "1")
                    session:sleep(70);
                    session:speak(d);
                    session:sleep(200)
                    session:setVariable("playing_prompt", "0")
                    session:setVariable("termination_reason", "1") 
                    session:hangup()
                end 
            end
        else 
            freeswitch.consoleLog("WARNING","AAI EVENT RECEIVED - transcription - empty - UUID:" .. received_aai_event:getHeader("Channel-Call-UUID") .. "\n") 
        end
    -- elseif(received_event and received_event:getHeader("Channel-Call-UUID") == uuid) then 
    --     body = received_event:getBody() 
    --     freeswitch.consoleLog("WARNING","EVENT RECEIVED - body: " .. body .. " UUID:" .. received_event:getHeader("Channel-Call-UUID") .. "\n") 
    --     -- freeswitch.consoleLog("WARNING","EVENT RECEIVED(uuid: " .. uuid .. ") - body: " .. body .. " transcription_event_uuid: " .. received_event:getHeader("Channel-Call-UUID") .. "\n") 
    --     T1 = JSON:decode(body)
    --     if (T1.is_final == true) then

    --         -- Got a transcript! Let's play a beep tone to a caller;
    --         local beepFile = "/usr/local/freeswitch/sounds/Blip.wav"
    --         session:streamFile(beepFile)

    --         -- Send text to Aida
    --         text = T1.alternatives[1].transcript
    --         freeswitch.consoleLog("WARNING","Text for Aida-Docker:" ..  text .. "UUID:" .. received_event:getHeader("Channel-Call-UUID") .. "\n") 

    --         -- Send updateSession with new text string 
    --         text = text:gsub( " ", "%%20" )
    --         url1 = aida_server .. "/sessions/update/" .. session_token .. "/" .. text
    --         freeswitch.consoleLog("WARNING","SESSION UPDATE:" .. url1 .. "\n") 
    --         -- /updateSession
    --         session:execute("curl", url1)
    --         curl_response_code = session:getVariable("curl_response_code")
    --         curl_response      = session:getVariable("curl_response_data")
    --         if (curl_response_code == "200") then
    --             -- We need to check how many params response has:
    --             -- 2 params - play message and wait for a caller input
    --             -- 3 params - transfer call - play a messge and transfer the call to a number (3rd params)
    --             -- en-AU-Wavenet-C&Ok.I found him, he can talk to you&+61234567
    --             -- session:execute("transfer", "3000 XML default")  -- we need to exit the session after call transfer; won't be exited automatically
    --             freeswitch.consoleLog("WARNING","Aida response:" .. curl_response ..  " UUID:" .. uuid .. "\n") 
                
    --             wavenet, aidaResponse, numberToTransfer = curl_response:match("(.+)&(.+)&?(.*)")
    --             if aidaResponse == nil or aidaResponse == "" then
    --                 aidaResponse = "Please wait a moment..."
    --             end
    --             freeswitch.consoleLog("WARNING","wavenet:" .. wavenet .. " aidaResponse:" .. aidaResponse .. "\n") 
    --             session:setAutoHangup(false);
    --             session:set_tts_params("google_tts", wavenet);
    --             -- session:set_tts_params("google_tts", "en-GB-Wavenet-F");
    --             -- wiat if we already playing a prompt
    --             while (session:getVariable("playing_prompt") == "1") do
    --                 session:sleep(20)
    --             end
    --             session:setVariable("playing_prompt", "1")
    --             session:speak(aidaResponse);
    --             session:sleep(700)
    --             session:setVariable("playing_prompt", "0")
    --         else
    --             freeswitch.consoleLog("ERROR","CURL response_code:" .. curl_response_code .. " UUID:" .. uuid .. "\n") 
    --             if curl_response then 
    --                 freeswitch.consoleLog("ERROR","CURL response_data:" .. curl_response .. " UUID:" .. uuid .. "\n") 
    --                 -- Extract a message from curl_response
    --                 a,b = curl_response:match("(.+),(.+)")
    --                 freeswitch.consoleLog("WARNING","curl_response -a:" .. a .. " curl_response -b:" .. b .. "\n") 
    --                 c = ""
    --                 d = ""
    --                 if string.match(a, "error") then
    --                     c,d = b:match("(.+):(.+)")
    --                 else
    --                     c,d = a:match("(.+):(.+)")
    --                 end
    --                 freeswitch.consoleLog("WARNING","curl_response -c:" .. c .. " curl_response -b:" .. d .. "\n") 
    --                 session:speak(d);
    --             end 
    --         end
    --     end 
    else   
        if (received_event)  then
            freeswitch.consoleLog("WARNING","OTHER call - body: " .. received_event:getBody() .. "UUID:" .. received_event:getHeader("Channel-Call-UUID") .. "\n") 
        elseif(received_event and received_aai_event:getHeader("Channel-Call-UUID") == uuid) then 
            body = received_aai_event:getBody() 
            freeswitch.consoleLog("WARNING","EVENT RECEIVED(body: " .. body .. "UUID:" .. received_aai_event:getHeader("Channel-Call-UUID") .. "\n") 
    
        else
            freeswitch.consoleLog("WARNING","NO EVENT\n") 
        end
    end 
end

-- reply = api:executeString("bgapi uuid_google_transcribe " .. uuid .. " stop") 
-- freeswitch.consoleLog("info","END CALL - uuid_google_transcribe STOP - reply: " .. reply .."\n") 
reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " stop") 
freeswitch.consoleLog("info","END CALL - uuid_aai_transcription STOP - reply: " .. reply .."\n") 

-- End session API will be modified to add one more parameter  - 
-- status: 0 - transfer Ok ; 1 - the system hangup;  2 - caller hangup
reason = session:getVariable("termination_reason") 

url = aida_server .. "/sessions/end/" .. session_token .. "/" .. reason
freeswitch.consoleLog("info", "sessions/end: " .. url .. "\n") 

-------- /sessions/end/...
-------- session:execute("curl", url)
status = os.execute('curl ' .. url)
freeswitch.consoleLog("info","sessions/end status:" .. status .. "\n") 




