#! /bin/sh
set -e

# /etc/init.d/hostname.sh: set the machine name

PATH=/sbin:/bin

[ -f /etc/init.d/init-functions ] && . /etc/init.d/init-functions

case "$1" in
    start|"")
        # Suck in the new hostname file.
        [ -r /etc/hostname ] && HOSTNAME=`cat /etc/hostname`
        FILE_HOSTNAME="$HOSTNAME"
        # Keep the current hostname if there is none in the file.
        [ -z "$HOSTNAME" ] && HOSTNAME=`hostname`
        # Make up a random one
        [ -z "$HOSTNAME" ] && \
            HOSTNAME=minoca`od -An -tx2 -v -N2 /dev/urandom | cut -c1-3`

        log_begin_msg "Setting hostname to '$HOSTNAME'"

        # Save it back into the file so it will consistenly be that.
        if [ "$HOSTNAME" != "$FILE_HOSTNAME" ]; then
            echo "$HOSTNAME" > /etc/hostname || \
                log_warning_msg "Failed to update /etc/hostname"

        fi

        hostname -F /etc/hostname
        ES=$?
        log_end_msg $ES
        exit $ES
        ;;

    stop)
        # No-op
        ;;

    status)
        HOSTNAME=`hostname`
        if [ "$HOSTNAME" ]; then
            return 0

        else
            return 4
        fi

        ;;

    *)
        log_action_msg \
            "Usage: /etc/init.d/hostname.sh {start|stop|status}"

        exit 1
        ;;

esac

exit 0
