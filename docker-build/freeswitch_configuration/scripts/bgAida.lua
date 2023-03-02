function onInputCBF(s, _type, obj, arg)
    local k, v = nil, nil
    local _debug = true
    if _debug then
        for k, v in pairs(obj) do
            -- printSessionFunctions(obj)
            print(string.format('obj k-> %s v->%s\n', tostring(k), tostring(v)))
        end
        if _type == 'table' then
            for k, v in pairs(_type) do
                print(string.format('_type k-> %s v->%s\n', tostring(k), tostring(v)))
            end
        end
        print(string.format('\n(%s == dtmf) and (obj.digit [%s])\n', _type, obj.digit))
    end
    if (_type == "dtmf") then
        s:consoleLog("info", "session:onInputCBF - return break")
        return 'break'
    else
        s:consoleLog("info", "session:onInputCBF - return nothing ")
        return ''
    end
end

local api = freeswitch.API()
local bg_uuid = argv[1]
local session = freeswitch.Session(bg_uuid)
 
if session:ready() then
    -- session:setInputCallback('onInputCBF', '');
    -- syntax is session:recordFile(file_name, max_len_secs, silence_threshold, silence_secs)
    max_len_secs = 2000
    -- max_len_secs = 10
    silence_threshold = 100
    silence_secs = 25
    -- silence_secs = 5
    filename = session:getVariable('sounds_dir') .. "/" .. bg_uuid .. ".wav"
    session:consoleLog("info","bgAida -  start - readAndStream:" .. filename .. "\n") 
    test = session:readAndStream(filename, max_len_secs, silence_threshold, silence_secs);
    -- test = session:recordFile(filename, max_len_secs, silence_threshold, silence_secs);
    session:consoleLog("info", "bgAida -  start - readAndStream return: " .. test )
end
session:consoleLog("info", "bgAida - readAndStream() - exiting")
-- session:hangup()
-- while (session:ready() == true) do                                
--     session:consoleLog("info", "session:recordFile() - BACKGROUND!!!!")
 
--  end



