script_root = debug.getinfo(1, "S").source:match("@?(.*/)")

JSON = (loadfile(script_root .. "json.lua"))()

if event:getHeader("VM-Action") ~= "leave-message" then return end

url = freeswitch.getGlobalVariable("api_server")

user = event:getHeader("VM-User")
domain = event:getHeader("VM-Domain")
file_path = event:getHeader("VM-File-Path")

message_length = event:getHeader("VM-Message-Len")
timestamp = event:getHeader("VM-Timestamp")

caller_id_name = event:getHeader("VM-Caller-ID-Name")
caller_id_number = event:getHeader("VM-Caller-ID-Number")

-- needs auth

freeswitch.API():execute(
  "curl",
  url ..
    "/voicemail content-type application/json post " ..
    JSON:encode({
      file = file_path,
      phone_number = user,
      domain = domain,
      message_length = message_length,
      caller = {
        name = caller_id_name,
        number = caller_id_number
      }
    })
)

freeswitch.consoleLog("notice", "Voicemail information forwarded for handling " .. (file_path or "") .. " received for " .. (user or "unknown recipient"))

  -- List of 'unknown caller names'
  -- ''
  -- 'UNKNOWN'
  -- 'UNASSIGNED'
  -- 'WIRELESS CALLER'
  -- 'TOLL FREE CALL'
  -- 'Anonymous'
  -- 'Unavailable'

