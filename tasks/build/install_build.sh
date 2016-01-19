#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
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
if test x$DEBUG = xchk; then
    DEBUG_FLAG='-D'
fi

echo "Running msetup -v $DEBUG_FLAG --autodeploy"
msetup -v $DEBUG_FLAG --autodeploy
echo "Done running setup."

