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

--
-- JSON = (loadfile(script_root .. "json.lua"))()
pathsep = '/' 
-- Windows users do this instead: 
-- pathsep = '\' 
-- freeswitch.consoleLog("debug","before creating the API object\n")   
-- api = freeswitch.API()   
-- freeswitch.consoleLog("debug","after creating the API object\n")   
-- reply = api:executeString("version") 
-- freeswitch.consoleLog("debug","reply is: " .. reply .."\n") 
-- reply = api:executeString("status") 
-- freeswitch.consoleLog("debug","reply is: " .. reply .."\n") 
-- reply = api:executeString("sofia status") 
-- freeswitch.consoleLog("WARNING","reply is: " .. reply .."\n") 

-- reply = api:executeString("bgapi uuid_aai_transcription " .. uuid .. " start wss://api.assemblyai.com/v2/realtime/ws?sample_rate=8000") 

-- Custom vocabulary

-- python example
-- sample_rate = 16000
-- word_boost = ["foo", "bar"]
-- params = {"sample_rate": sample_rate, "word_boost": json.dumps(word_boost)}
-- url = f"wss://api.assemblyai.com/v2/realtime/ws?{urlencode(params)}"

local personString = '{"name": "Squid", "job": "dev"}' -- A JSON string
-- If you need to turn the json into an object:
local personJSON = JSON:decode(personString)
-- freeswitch.consoleLog("debug","personJSON:" .. personJSON .. "\n") 
-- Now you can use it like any table:
personJSON["name"] = "Illusion Squid"
personJSON.job = "Software Engineer"
-- If you have an object and want to make it a JSON string do this:
local newPersonString = JSON:encode(personJSON)
freeswitch.consoleLog("debug","newPersonString:" .. newPersonString .. "\n") 
-- Simple!

sample_rate = 8000
word_boost = '["Perth", "Nedlands", "Tuart Hill", "North Perth", "Cottesloe"]' 
json_word_boost = JSON:decode(word_boost)
word_boost_encoded = JSON:encode(json_word_boost)
freeswitch.consoleLog("debug","word_boost_encoded:" .. word_boost_encoded .. "\n") 

 params = { 
    ["sample_rate"] = sample_rate,
    -- ["word_boost"] =  json_word_boost
    ["word_boost"] =  word_boost
}

local suburbs_list = '['
local point_file = io.open("/usr/local/freeswitch/scripts/suburbs.txt", "r")
for line in point_file:lines() do
    -- freeswitch.consoleLog("debug", line .. "\n")
    suburbs_list = suburbs_list .. '"' .. line .. '",' 
    --  table.insert(line_data, player_data)
end
suburbs_list = suburbs_list:sub(1, -2)
suburbs_list = suburbs_list .. ']'
freeswitch.consoleLog("debug","suburbs_list:" .. suburbs_list .. "\n") 
-- json_list = JSON:decode(suburbs_list)
-- suburbs_list_encoded = JSON:encode(json_list)
-- freeswitch.consoleLog("debug","suburbs_list_encoded:" .. suburbs_list_encoded .. "\n") 



local paramsString = '{"sample_rate": 8000, "word_boost":' .. suburbs_list .. '}' -- A JSON string

json_params = JSON:decode (paramsString)
paramsStringEncoded = JSON:encode (json_params)
freeswitch.consoleLog("debug","paramsStringEncoded:" .. paramsStringEncoded .. "\n") 

url = "wss://api.assemblyai.com/v2/realtime/ws?" .. paramsStringEncoded
freeswitch.consoleLog("debug","URL:" .. url .. "\n") 


-- text = "sometext1234567890"
-- freeswitch.consoleLog("debug","sometext:" .. string.sub(text, ( #text - 1 ))  .. "\n") 
-- freeswitch.consoleLog("debug","sometext1:" .. string.sub(1, ( #text - 1 ))  .. "\n") 
-- freeswitch.consoleLog("debug","sometext2:" .. string.sub(text, ( #text - 1 ))  .. "\n") 
-- -- print( string.sub(text, ( #text - 1 )) )

-- vr_list = '["Perth","Darwin","Brisbane","Sydney","Melbourne","Wollongong","Adelaide","Canberra","Cairns","Carnarvon","Mandurah","Cottesloe","Toorak","Vauclause","Geelong","Ballarat","Lauceston","Fremantle","Hobart","Norfolk","Tasmania","Nedlands","Tuart Hill","North Perth","Mount Hawthorn","Yokine","Joondalup","Mount Claremont","Booragoon","Kalamunda","Balcatta","Osborne Park","Scarborough"]' 
-- json_vr_list = JSON:decode (vr_list)
-- vr_listEncoded = JSON:encode (json_vr_list)
-- freeswitch.consoleLog("debug","vr_listEncoded:" .. vr_listEncoded .. "\n") 

-- params = "{\"sample_rate\"\: " ..  sample_rate .. ", \"word_boost\"\: " .. json_word_boost .. "}"



local matches = {}

-- local function contains(str, pattern)
--   return string.match(str, pattern) and true or false
-- end

-- local file_path = "/usr/local/freeswitch/scripts/suburbs.txt" -- # path to your file here
-- for line in io.lines(file_path) do
--     table.insert(matches, line)
-- end
-- json_vr_list = JSON:decode (matches)
-- vr_listEncoded = JSON:encode (json_vr_list)
-- freeswitch.consoleLog("debug","matches:" .. vr_listEncoded .. "\n") 



-- encoded_params = encodeparams(params)
url = "wss://api.assemblyai.com/v2/realtime/ws?" .. JSON:encode (json_params)
-- freeswitch.consoleLog("debug","URL:" .. url .. "\n") 




