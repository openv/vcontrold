#!/bin/sh
#
# crond
#
#exit

case $1 in

	start)
		echo "vcontrol starting"
		/usr/sbin/vcontrold
		;;

esac

