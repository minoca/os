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
##     update-rc.d
##
## Abstract:
##
##     This script provides a poor man's version of the real update-rc.d. This
##     script does not read or understand dependencies.
##
## Author:
##
##     Evan Green 24-Mar-2015
##
## Environment:
##
##     POSIX
##

me=$0
do_it=
force=
name=

usage () {
    echo "Usage: $me [-n] [-f] name command"
    echo "Commands:"
    echo "    defaults [NN | SS KK]"
    echo "    remove"
    echo "    start|stop NN runlevel... . start|stop NN runlevel... . ..."
    echo "    enable|disable [S|2|3|4|5]"
    exit 1
}

validate_sequence () {
    case "$1" in
    [0123456789][0123456789]) return 0 ;;
    *) return 1 ;;
    esac
}

validate_runlevels () {
    local levels
    levels=`echo $1 | sed 's/\(.\)/\1 /g'`
    for level in $levels; do
        case $level in
        [0123456S])
            ;;

        *)
            echo "$me: Error: Invalid runlevel $level."
            exit 1
            ;;

        esac
    done
}

check_name () {
    local name
    name="$1"
    if [ ! -f $SYSROOT/etc/init.d/$name ]; then
        echo "$me: Error: $SYSROOT/etc/init.d/$name does not exist."
        exit 1
    fi
}

create_links () {
    local action name sequence runlevels levels link_dest
    action="$1"
    name="$2"
    sequence="$3"
    runlevels="$4"
    levels=`echo $runlevels | sed 's/\(.\)/\1 /g'`
    if ! validate_sequence "$sequence"; then
        exit 1
    fi

    if ! validate_runlevels "$runlevels"; then
        exit 1
    fi

    ##
    ## Create the links.
    ##

    for level in $levels; do
        mkdir -p $SYSROOT/etc/rc$level.d
        link_dest=$SYSROOT/etc/rc$level.d/$action$sequence$name

        ##
        ## Only create the link if nothing already exists there.
        ##

        if [ -z "$force" ] && [ -f "$link_dest" -o -L "$link_dest" ]; then
            echo "$me: Skipping pre-existing $link_dest."

        else
            $do_it cd $SYSROOT/etc/rc$level.d
            $do_it ln -sf "../init.d/$name" "$link_dest"
        fi
    done
}

rename_links() {
    local src_prefix dst_prefix name levels runlevels suffix link_dest seq rcn
    src_prefix="$1"
    dst_prefix="$2"
    name="$3"
    runlevels="$4"
    if ! validate_runlevels "$runlevels"; then
        exit 1
    fi

    levels=`echo $runlevels | sed 's/\(.\)/\1 /g'`
    for level in $levels; do
        if [ ! -d $SYSROOT/etc/rc$level.d/ ]; then
            continue
        fi

        rcn=$SYSROOT/etc/rc$level.d
        for f in $rcn/$src_prefix[0123456789][0123456789]$name; do
            suffix=${f#$rcn/$src_prefix}
            seq=${suffix%$name}
            seq=$((100 - $seq))
            link_dest=$rcn/$dst_prefix$seq$name

            ##
            ## Only move the link if nothing already exists there.
            ##

            if [ -f "$link_dest" -o -L "$link_dest" ]; then
                echo "$me: Skipping pre-existing $link_dest."

            else
                $do_it mv "$f" "$link_dest"
            fi
        done
    done
}

##
## Process arguments.
##

while [ -n $1 ]; do
    case "$1" in
    -n)
        do_it=echo
        ;;

    -f)
        force=yes
        ;;

    *)
        break
        ;;

    esac

    shift
done

name=$1
if [ -z $name ]; then
    usage
fi

shift

##
## Loop processing action arguments. Technically only start and stop are
## supposed to be able to keep going, but that's okay.
##

while [ -n "$1" ] ; do
    action=$1
    shift
    case "$action" in

    ##
    ## Remove all symbolic links which point to /etc/init.d/<name>.
    ##

    remove)
        script=$SYSROOT/etc/init.d/$name
        if [ "$force" != "yes" ]; then
            if [ -f $script ]; then
                echo "$me: Error: $script still exists!"
                exit 1
            fi
        fi

        ##
        ## Touch the file to make it exist so the symbolic link destination
        ## can be checked.
        ##

        touch $script
        for level in 0 1 2 3 4 5 6 S; do
            if [ ! -d $SYSROOT/etc/rc$level.d/ ]; then
                continue
            fi

            for f in $SYSROOT/etc/rc$level.d/*; do
                if [ $f -ef $script ]; then
                    $do_it rm $f
                fi
            done
        done

        ##
        ## If the script is just an empty file (created by the touch), then
        ## remove it.
        ##

        if [ -s $script ]; then
            rm $script
        fi

        ;;

    ##
    ## Create start scripts at sequence 20, runlevels 2345, and kill scripts
    ## at sequence 80, runlevels 016.
    ##

    defaults)
        check_name "$name"
        start_seq=20
        kill_seq=80
        if [ -n "$1" ]; then
            if [ -n "$2" ]; then
                start_seq="$1"
                kill_seq="$2"
                shift

            else
                start_seq="$1"
                kill_seq="$1"
            fi

            shift
        fi

        start_runlevels="2345"
        kill_runlevels="016"
        create_links "S" "$name" "$start_seq" "$start_runlevels"
        create_links "K" "$name" "$kill_seq" "$kill_runlevels"
        ;;

    ##
    ## Create the specified start/stop scripts.
    ##

    start|stop)
        check_name "$name"
        if [ -z "$1" ]; then
            echo "$me: Error: sequence number missing after start/stop."
            usage
            exit 1
        fi

        sequence="$1"
        shift
        runlevels=""
        while [ "$1" != "." ]; do
            if [ -z "$1" ]; then
                echo "$me: Error: Expected runlevel or period for start/stop."
                usage
                exit 1
            fi

            runlevels="$runlevels$1"
            shift
        done

        ##
        ## Shift away the period.
        ##

        shift

        ##
        ## Create the desired scripts.
        ##

        action_code="K"
        if [ "$action" = "start" ]; then
            action_code="S"
        fi

        create_links "$action_code" "$name" "$sequence" "$runlevels"
        ;;

    enable|disable)
        runlevels="2345"
        if [ -n "$1" ]; then
            runlevels="$1"
            shift
        fi

        if [ "$action" = "enable" ]; then
            rename_links "K" "S" "$name" "$runlevels"

        else
            rename_links "S" "K" "$name" "$runlevels"
        fi
        ;;

    *)
        echo "$me: Error: Unexpected argument $action."
        exit 1
        ;;

    esac
done

exit 0
