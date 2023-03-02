#!/bin/bash
KEY_PAIR=~/.ssh/ecs-freeswitch-cluster
    ecs-cli up \
      --keypair $KEY_PAIR  \
      --capability-iam \
      --size 2 \
      --instance-type t3.medium \
      --tags project=ecs-freeswitch-cluster,owner=Voj \
      --cluster-config ecs-config \
      --ecs-profile ecs-profile

      