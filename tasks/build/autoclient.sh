#! /bin/sh
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
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
ARGS="/auto/client.py"
if [ -n $2 ]; then
    ARGS="$ARGS $2"
fi

##
## Start in a different directory if an alternate root was specified. Perhaps
## the OS partition is not big enough to support builds.
##

AUTO_ROOT=/auto
if [ -r /auto/auto_root ]; then
    AUTO_ROOT=`cat /auto/auto_root`
fi

case "$1" in
    start)

        ##
        ## Copy files from /auto if an alternate root was specified.
        ##

        if [ "$AUTO_ROOT" != "/auto" ]; then
            if ! [ -d "$AUTO_ROOT/" ]; then
                mkdir "$AUTO_ROOT/"
            fi

            if ! [ -r "$AUTO_ROOT/auto_root" ]; then
                log_daemon_msg "Copying Minoca Build client files" || true
                cp -Rpv /auto/* "$AUTO_ROOT/"
                log_end_msg 0 || true
            fi
        fi

        ##
        ## Fire up the build client.
        ##

        log_daemon_msg "Running Minoca Build client" || true
        if start-stop-daemon -S -p /var/run/mbuild.pid -d "$AUTO_ROOT" -bqomC \
            -x /usr/bin/python -- $ARGS \
            >>/var/log/autoclient.log 2>&1; then

            log_end_msg 0 || true

        else
            log_end_msg 1 || true

        fi
        ;;

    stop)
        log_daemon_msg "Stopping Minoca Build client" || true
        if start-stop-daemon -K -p /var/run/mbuild.pid -qo -n python; then

            log_end_msg 0 || true

        else
            log_end_msg 1 || true
        fi
        ;;

    reload|force-reload|restart)
        $0 stop
        $0 start
        ;;

    *)
        log_action_msg "Usage: $0 {start|stop|reload|force-reload|restart}"
        exit 1
        ;;
esac
exit 0

