script_root = debug.getinfo(1, "S").source:match("@?(.*/)")

local JSON = (loadfile(script_root .. "json.lua"))()


local to_hex = function(c)
  return string.format("%%%02X", string.byte(c))
end


session:answer();

session:setAutoHangup(false);


deleted_files = {}
delete_sent = false

url = freeswitch.getGlobalVariable("api_server")

user = session:getVariable("userbox") or "none"

user_param = user:gsub("([^-%w ._~])", to_hex)

files = {'abc'}

function JSON:assert(message)
  -- We don't care about this error, just move on it will cause the system to exit,
  -- but we don't need to explode to get out. At least say goodbye. ;)
end

files = JSON:decode(
  freeswitch.API():execute(
    "curl",
      url .. "/voicemail?" ..
      "phone_number=" .. user_param ..
    " get"
  )
)

freeswitch.consoleLog("CONSOLE", "voicemails: " .. JSON:encode(files));

function finaliseTuiActions(s, status)
  delete_sent = true

  call_content = {}
  for k in pairs(deleted_files) do
    call_content[1 + #call_content] = k
  end

  freeswitch.consoleLog("CONSOLE", "deleting: " .. JSON:encode(call_content));

  freeswitch.API():execute(
    "curl",
    url .. "/voicemail/complete_tui?" ..
    "phone_number=" .. user_param ..
    " content-type application/json " ..
    " POST " ..
    JSON:encode({
      delete = call_content
    })
  );

  return "exit";
end

-- If the user hangs up during the call
session:setHangupHook("finaliseTuiActions")

local number = function(value)
 return freeswitch.API():execute("say_string", "en.wav en number pronounced " .. value);
end

local playMessage = function(message_number)
  messageDateSound = freeswitch.API():execute("say_string", "en.wav en short_date_time pronounced " .. files[message_number].time);

  if message_number == #files then
    nextMessagePart = ""
  else
    nextMessagePart = "!voicemail/vm-play_next_message.wav!voicemail/vm-press.wav!digits/9.wav"
  end

  return session:read(
    1, 1,
    "voicemail/vm-received.wav!" ..
    messageDateSound ..
    "!silence_stream://500,1400!" ..
    files[message_number].file ..
    "!silence_stream://500,1400" ..
    "!voicemail/vm-listen_to_recording_again.wav!voicemail/vm-press.wav!digits/1.wav" ..
    -- if it's deleted already, change this to undelete = 4?
    "!voicemail/vm-delete_recording.wav!voicemail/vm-press.wav!digits/3.wav" ..
    nextMessagePart ..
    "!silence_stream://2000,1400!"
    , 10, "#"
  );
end


message_number = 0

local mainMenu = function(play_greeting_message)
  messageCountSound = number(#files);

  if #files == 1 then
    messages = "!voicemail/vm-message.wav"
  else
    messages = "!voicemail/vm-messages.wav"
  end

  if play_greeting_message then
    keyed = session:read(
      1, 1, "silence_stream://500,1400!voicemail/vm-you_have.wav!" .. messageCountSound .. messages, 1000, "#"
    );
  end

  if message_number == #files then return "end" end

  if keyed == "#" then
    return "end";
  elseif keyed ~= "" and keyed < #files then
    message_number = keyed
  elseif message_number < #files then
    message_number = message_number + 1
  end

  result = playMessage(message_number);

  if result == "1" then
    message_number = message_number - 1;
  elseif result == "3" then
    session:streamFile("silence_stream://500,1400!voicemail/vm-message.wav!voicemail/vm-deleted.wav");
    deleted_files[files[message_number].message_id] = 1
  end -- nine is just an early escape

  -- if result then return "end";

  -- if the message was deleted, and it was the last message, replay the mainMenu

  return false;
end

if files then
  play_greeting_message = true;
  while play_greeting_message ~= "end" do
    play_greeting_message = mainMenu(play_greeting_message);
  end
  session:streamFile("silence_stream://500,1400!voicemail/vm-goodbye.wav");

  finaliseTuiActions(0,0);
else
  -- you're in the wrong place, bye
  session:streamFile("silence_stream://500,1400!voicemail/vm-goodbye.wav");
end