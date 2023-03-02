local api = freeswitch.API()
local call_uuid         = argv[1]
local wavenet           = argv[2]
local prompt            = argv[3]
local command           = argv[4]

local session = freeswitch.Session(call_uuid)

if call_uuid and wavenet and prompt and command then
    session:consoleLog("debug", "call_handler.lua - uuid: " .. call_uuid .. " wavenet: " .. wavenet .. " prompt: " .. prompt .. " cmd:" .. command .. "\n")
end

if session:ready() then
    -- freeswitch.consoleLog("info","wavenet:" .. wavenet .. " prompt:" .. prompt .. " command:" .. command .. "\n") 
    session:setAutoHangup(false);

    -- wait if currently playing a message to an user
    while (session:getVariable("playing_prompt") == "1") do
        session:sleep(50)
    end
    session:setVariable("playing_prompt", "1")

    -- command: 1 - end call; 2 - inject prompt; 3- ?
    if command == "1" then 
        session:setVariable("hangup_in_progress", "1") 
        session:setAutoHangup(true);
        -- sched_hangup [+]<time> <uuid> [<cause>]
        -- sched_hangup +0 is the same as uuid_kill
        -- session:execute("sched_hangup +0 " .. call_uuid)
        -- session:execute("uuid_kill " .. call_uuid)
        reply = api:executeString("bgapi uuid_aai_transcription " .. call_uuid .. " stop") 
        -- reply = api:executeString("bgapi uuid_google_transcribe " .. call_uuid .. " stop") 
        session:consoleLog("info","bgapi uuid_aai_transcription STOP - reply: " .. reply .."\n") 
        session:consoleLog("debug", "call_handler.lua - hangup()\n")

        session:sleep(100)
        session:set_tts_params("google_tts", wavenet);
        prompt = prompt:gsub( "_", "'" )
        session:speak(prompt);
        session:sleep(700)
        session:setVariable("playing_prompt", "0")
        -- set termination_reason variable: 0 - transfer Ok ; 1 - the system hangup;  2 - caller hangup
        session:setVariable("termination_reason", "1") 
        session:hangup()
    elseif command == "2" then
        if (session:getVariable("speaking_state") == "0") and (session:getVariable("session_updating") == "0") then 
            session:consoleLog("debug", "call_handler.lua - play msg: " .. prompt .. "\n")
            session:sleep(200)
            session:set_tts_params("google_tts", wavenet);
            prompt = prompt:gsub( "_", "'" )
            session:speak(prompt);
            session:sleep(700)
            -- session:setVariable("playing_prompt", "0")
        end
    else
        session:consoleLog("error", "Command not supported:" ..  command .. "\n")
    end
    session:setVariable("playing_prompt", "0")
end
session:consoleLog("info", "call_handler.lua - exiting session!")

