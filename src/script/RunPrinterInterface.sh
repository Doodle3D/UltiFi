#!/bin/sh

sleep 1
/UltiFi/ManageConnection.bin $1
rm -rf /tmp/UltiFi/`basename $1`
