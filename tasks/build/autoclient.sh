#! /bin/sh
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     autoclient.sh start|stop|reload|force-reload|restart
##
## Abstract:
##
##     This init script starts the Minoca build client.
##
## Author:
##
##     Evan Green 24-Mar-2015
##
## Environment:
##
##     Build
##

set -e

##
## /etc/init.d/autoclient.sh: start and stop the Minoca build client script.
##

[ -f /etc/init.d/init-functions ] && . /etc/init.d/init-functions

umask 022
ARGS="./client.py"
if [ -n $2 ]; then
    ARGS="$ARGS $2"
fi

case "$1" in
    start)
        log_daemon_msg "Running Minoca Build client" || true
        if start-stop-daemon -S -p /var/run/mbuild.pid -d /auto -bqom \
            -x /usr/bin/python -- $ARGS ; then

            log_end_msg 0 || true

        else
            log_end_msg 1 || true

        fi
        ;;

    stop)
        log_daemon_msg "Stopping Minoca Build client" || true
        if start-stop-daemon -K -p /var/run/mbuild.pid -qo -n python \
            -x /usr/bin/python; then

            log_end_msg 0 || true

        else
            log_end_msg 1 || true
        fi
        ;;

    reload|force-reload|restart)
        log_daemon_msg "Restarting Minoca Build client"
        start-stop-daemon -K -p /var/run/mbuild.pid -qo -n python
        sleep 2
        if start-stop-daemon -S -p /var/run/mbuild.pid -d /auto -bqom \
            -x /usr/bin/python -- $ARGS ; then

            log_end_msg 0 || true

        else
            log_end_msg 1 || true
        fi
        ;;

    *)
        log_action_msg "Usage: $0 {start|stop|reload|force-reload|restart}"
        exit 1
        ;;
esac
exit 0

