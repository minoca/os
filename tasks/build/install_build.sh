#!/bin/sh
## Copyright (c) 2014 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     install_build.sh
##
## Abstract:
##
##     This script installs the previously extracted latest build.
##
## Author:
##
##     Evan Green 19-May-2014
##
## Environment:
##
##     Minoca Build
##

set -e

if test -z "$ARCH"; then
    echo "ARCH must be set."
    exit 1
fi

if test -z "$DEBUG"; then
    echo "DEBUG must be set."
    exit 1
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR

##
## Make sure the appropriate files have been extracted.
##

error=''
if ! test -d ./bin; then
    error="Missing bin directory."
fi

if ! test -x ./bin/msetup; then
    error="Missing setup."
fi

if ! test -r ./bin/install.img; then
    error="Missing install.img."
fi

if test -n "$error"; then
    echo $error
    pwd
    echo ls -la .
    ls -la .
    echo ls -la ./bin
    ls -la ./bin
    exit 1
fi

cd ./bin

##
## Determine whether or not to install the target with kernel debugging enabled.
##

DEBUG_FLAG=''
if test "$DEBUG" != "rel"; then
    DEBUG_FLAG='-D'
fi

##
## If an alternate root was specified, then add that to the build.
##

AUTO_ROOT_ARGS=
if [ -n "$AUTO_ROOT" ]; then
    AUTO_ROOT_ARGS="--script=./auto_root_script"
    echo "$AUTO_ROOT" > ./auto_root

    ##
    ## This script copies the auto_root file in the current directory to
    ## /auto/auto_root at the setup destination.
    ##

    cat > ./auto_root_script <<_EOS
var AutoRootCopy = {
    "Destination": "/apps/auto/auto_root",
    "Source": "./auto_root",
    "SourceVolume": -1,
};

SystemPartition["Files"] += [AutoRootCopy];
_EOS

fi

if [ -r /etc/hostname ]; then
    cat > ./keep_hostname <<_EOS
var HostnameCopy = {
    "Destination": "/apps/etc/hostname",
    "Source": "/etc/hostname",
    "SourceVolume": -1,
};

SystemPartition["Files"] += [HostnameCopy];
_EOS

AUTO_ROOT_ARGS="$AUTO_ROOT_ARGS --script=./keep_hostname"
fi

echo "Running msetup -v $DEBUG_FLAG $AUTO_ROOT_ARGS --autodeploy -a3072"
msetup -v $DEBUG_FLAG $AUTO_ROOT_ARGS --autodeploy -a3072
echo "Done running setup."

