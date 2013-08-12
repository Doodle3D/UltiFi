#!/bin/sh

sleep 1
ULTIFI_PATH=/tmp/UltiFi/`basename $1`
mkdir -p $ULTIFI_PATH
/UltiFi/ManageConnection.bin $1 2> $ULTIFI_PATH/server.log
rm -rf $ULTIFI_PATH
