local api = freeswitch.API()
local call_uuid         = argv[1]
local wavenet           = argv[2]
local aidaResponse      = argv[3]
local numberToTransfer  = argv[4]

local session = freeswitch.Session(call_uuid)

if call_uuid then
    session:consoleLog("info", "transfer.lua - call_uuid: " .. call_uuid .."\n")
end 

if session:ready() then
    freeswitch.consoleLog("info","wavenet:" .. wavenet .. " aidaResponse:" .. aidaResponse .. " numberToTransfer:" .. numberToTransfer .. "\n") 
    session:setAutoHangup(false);

    while (session:getVariable("playing_prompt") == "1") do
        session:sleep(100)
    end
    session:setVariable("playing_prompt", "1")
    
    session:set_tts_params("google_tts", wavenet);
    -- session:set_tts_params("google_tts", "en-GB-Wavenet-F");
    session:speak(aidaResponse);
    if (numberToTransfer ~= nil and numberToTransfer ~= '') then 
    -- if (numberToTransfer ~= nil and called_number == "+61878284599") then 
    -- if (numberToTransfer ~= nil and called_number == "+6421234565") then 
        reply = api:executeString("bgapi uuid_google_transcribe " .. call_uuid .. " stop") 
        if reply then
            freeswitch.consoleLog("info","uuid_google_transcribe STOP - reply: " .. reply .."\n") 
        end

        -- set termination_reason variable: 0 - transfer Ok ; 1 - the system hangup;  2 - caller hangup
        session:setVariable("termination_reason", "0") 
        session:sleep(1000)
        -- session:execute("transfer", numberToTransfer .. " XML default")  -- we need to exit the session after call transfer; won't be exited automatically
        -- session:execute("transfer", "+61488197252 XML voicemail")  -- we need to exit the session after call transfer; won't be exited automatically
        numberToTransfer = "T" .. numberToTransfer
        session:execute("transfer", numberToTransfer .. " XML voicemail")  -- we need to exit the session after call transfer; won't be exited automatically
        session:sleep(500)
        session:setVariable("playing_prompt", "0")
        -- break
    else
        freeswitch.consoleLog("warning","There is no number to transfer call") 
    end

end
session:consoleLog("info", "session:transfer - exiting session!")




