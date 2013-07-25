#!/bin/sh

sleep 20
while true; do 
	for DEV in $(ls /dev/ttyACM*); do
		DEVBASE=`basename ${DEV}`
		if [ ! -d /tmp/UltiFi/${DEVBASE} ]; then
			/UltiFi/RunPrinterInterface.sh ${DEV} &
			sleep 2
		fi
	done
	sleep 5
done
