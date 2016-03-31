#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     make.sh <directory> <make_arguments>
##
## Abstract:
##
##     This script runs make somewhere while inside the Minoca OS environment.
##     SRCROOT, DEBUG, and ARCH must be set.
##
## Author:
##
##     Evan Green 13-May-2014
##
## Environment:
##
##     Minoca (Windows) Build
##

set -e

if test -z "$SRCROOT"; then
    SRCROOT=`pwd`/src
fi

if test -z "$ARCH"; then
    echo "ARCH must be set."
    exit 1
fi

if test -z "$DEBUG"; then
    echo "DEBUG must be set."
    exit 1
fi

cd $SRCROOT
export SRCROOT=$PWD
export TMPDIR=$PWD
export TEMP=$TMPDIR
SOURCE_DIRECTORY=$1
shift
if test -z $SOURCE_DIRECTORY; then
    echo "first argument must be source directory."
fi

export PATH=$PATH:$SRCROOT/$ARCH$VARIANT$DEBUG/bin
cd $SOURCE_DIRECTORY

##
## If building OS and there's a revision file, then set the revision number
## to it.
##

if test "x$PWD" = "x$SRCROOT/os"; then
    if test -r "$SRCROOT/os/revision"; then
        export REVISION=`cat "$SRCROOT/os/revision"`
    fi
fi

uname -a
set
echo make "$@"
make "$@"
echo completed make "$@"
if test -z "$NOCLEAN" -a "x$SOURCE_DIRECTORY" != "xos"; then
    make clean
    echo completed make clean
fi

