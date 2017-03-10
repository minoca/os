##
## Copyright (c) 2014 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
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
    touch "$WORLD/dev/tty"
    mkdir -p "$WORLD/dev/Volume"
    mkdir -p "$WORLD/dev/Terminal"
    mkdir -p "$WORLD/dev/Devices"
    mount --bind "/Device/null" "$WORLD/dev/null"
    mount --bind "/Device/full" "$WORLD/dev/full"
    mount --bind "/Device/zero" "$WORLD/dev/zero"
    mount --bind "/Device/urandom" "$WORLD/dev/urandom"
    mount --bind "/Device/tty" "$WORLD/dev/tty"
    mount --bind "/Terminal/Slave0" "$WORLD/dev/console"
    mount --bind "/Volume" "$WORLD/dev/Volume"
    mount --bind "/Terminal" "$WORLD/dev/Terminal"
    mount --bind "/Device" "$WORLD/dev/Devices"
    mkdir -p "$WORLD/dev/Pipe"
    mount --bind "/Pipe" "$WORLD/dev/Pipe"
fi

##
## Set up the home environment.
##

mkdir -p "$WORLD/root" \
    "$WORLD/var/run" \
    "$WORLD/var/log" \
    "$WORLD/home"

export HOME="/root"
export TERM=xterm

##
## Clean out the tmp and dev directories.
##

rm -rf "$WORLD/tmp" "$WORLD/dev"
mkdir -p "$WORLD/dev"
mkdir -p -m1777 "$WORLD/tmp"

##
## Symlink swiss binaries.
##

if ! test -x $WORLD/bin/chroot; then

    ##
    ## If there is no swiss, move sh to swiss, setuid on swiss, and then link
    ## all the other binaries to swiss.
    ##

    if ! test -r $WORLD/bin/swiss ; then
        mv $WORLD/bin/sh $WORLD/bin/swiss
        chmod u+s $WORLD/bin/swiss
    fi

    for app in `swiss --list`; do
        if ! test -x $WORLD/bin/$app; then
            ln -s swiss $WORLD/bin/$app
        fi
    done

    ##
    ## Also check on root's home and ssh directories, as having those set wrong
    ## prevents logging in via SSH.
    ##

    chmod -f go-rwx $WORLD/root $WORLD/root/.ssh \
        $WORLD/root/.ssh/authorized_keys \
        $WORLD/root/.ssh/id_rsa

fi

##
## Run the final user shell.
##

exec $WORLD/bin/chroot "$WORLD" -- /bin/init

