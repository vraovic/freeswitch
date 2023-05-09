#!/bin/sh

# Save the variable value to a file
echo "$GOOGLE_APPLICATION_API">>/usr/local/freeswitch/freeswitch-api-7867f30c80d7.json

freeswitch -nonat -conf /usr/local/freeswitch/conf -log /usr/local/freeswitch/log -db /usr/local/freeswitch/db -scripts /usr/local/freeswitch/scripts -storage /usr/local/freeswitch/store
