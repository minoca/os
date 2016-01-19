##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     init.sh
##
## Abstract:
##
##     This script performs user mode initialization.
##
## Author:
##
##     Evan Green 20-Dec-2014
##
## Environment:
##
##     Minoca OS
##

##
## Define the location to set up and ultimately chroot into. If such a world
## does not exist, then try to fire up a shell directly.
##

if test -z "$WORLD"; then
    WORLD="apps"
fi

if ! test -d "$WORLD"; then
    if test -z "$CONSOLE"; then
        CONSOLE=/Terminal/Slave0
    fi

    0<>$CONSOLE
    1>$CONSOLE
    2>$CONSOLE
    export LD_LIBRARY_PATH=$PWD
    exec $PWD/swiss sh -i
fi

##
## Set up some working environment variables.
##

export LD_LIBRARY_PATH="$PWD/$WORLD/lib"
export PATH="$PWD/$WORLD/bin"

##
## Mount the special devices if needed.
##

if ! test -c "$WORLD/dev/null"; then
    mkdir -p "$WORLD/dev"
    touch "$WORLD/dev/null"
    touch "$WORLD/dev/full"
    touch "$WORLD/dev/zero"
    touch "$WORLD/dev/urandom"
    touch "$WORLD/dev/console"
    mkdir -p "$WORLD/dev/Volume"
    mkdir -p "$WORLD/dev/tty"
    mount --bind "/Device/null" "$WORLD/dev/null"
    mount --bind "/Device/full" "$WORLD/dev/full"
    mount --bind "/Device/zero" "$WORLD/dev/zero"
    mount --bind "/Device/urandom" "$WORLD/dev/urandom"
    mount --bind "/Terminal/Slave0" "$WORLD/dev/console"
    mount --bind "/Volume" "$WORLD/dev/Volume"
    mount --bind "/Terminal" "$WORLD/dev/tty"
fi

##
## Set up the home environment.
##

mkdir -p "$WORLD/root"
export HOME="/root"
export TERM=xterm

##
## Clean out the tmp directory.
##

rm -rf "$WORLD/tmp"
mkdir -p -m777 "$WORLD/tmp"

##
## Symlink swiss binaries.
##

if ! test -x $WORLD/bin/chroot; then

    ##
    ## If there is no swiss, link sh to swiss (hopefully that works). Pipes
    ## don't really work at this point.
    ##

    if ! test -r $WORLD/bin/swiss ; then
        ln -s sh $WORLD/bin/swiss
    fi

    for app in `swiss --list`; do
        if ! test -x $WORLD/bin/$app; then
            ln -s swiss $WORLD/bin/$app
        fi
    done
fi

##
## Run the final user shell.
##

exec $WORLD/bin/chroot "$WORLD" -- /bin/init

