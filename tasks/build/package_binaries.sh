#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     package_binaries.sh
##
## Abstract:
##
##     This script archives the native build artifacts.
##
## Author:
##
##     Evan Green 13-Jun-2014
##
## Environment:
##
##     Minoca Build
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

export TMPDIR=$PWD
export TEMP=$TMPDIR
ARCHIVE_LOCATION=$PWD/..

cd $SRCROOT/$ARCH$DEBUG

##
## Move all the packages over, so they get saved beyond this task and aren't in
## the archive.
##

cp "./bin/kernel-version" "./bin/packages/"
if test -d "./bin/packages"; then
    rm -rf "$ARCHIVE_LOCATION/packages/"
    mv "./bin/packages/" "$ARCHIVE_LOCATION/"
fi

##
## Archive the bin folder.
##

date > ./bin/build-date
cp $SRCROOT/os/revision ./bin/build-revision
echo $ARCH$DEBUG > ./bin/build-flavor
file=$ARCHIVE_LOCATION/minoca-bin-$ARCH$DEBUG.tar.gz
tar -cz -f $file ./bin
echo "Completed packaging $file"

