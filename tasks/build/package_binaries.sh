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

cd $SRCROOT/$ARCH$VARIANT$DEBUG

##
## Copy the automation files.
##

AUTOROOT="$PWD/bin/auto"
if ! test -d "$AUTOROOT"; then
    mkdir -p "$AUTOROOT/testbin"
    cp -v "$SRCROOT/client.py" "$AUTOROOT/client.py"
    cp -Rv "$SRCROOT/os/tasks/" "$AUTOROOT/"
    cp -v "bin/perftest" "$AUTOROOT/testbin/perftest"
    cp -v "bin/perflib.so" "$AUTOROOT/testbin/perflib.so"
fi

##
## Move all the packages over, so they get saved beyond this task and aren't in
## the archive.
##

cp "./bin/kernel-version" "./bin/packages"
if test -d "./bin/packages"; then
    rm -rf "$ARCHIVE_LOCATION/packages"
    mv "./bin/packages" "$ARCHIVE_LOCATION"
fi

##
## Remove unimportant stuff from the archive.
##

rm -rf ./bin/dep

##
## Archive the bin folder.
##

date > ./bin/build-date
cp $SRCROOT/os/revision ./bin/build-revision
echo $ARCH$VARIANT$DEBUG > ./bin/build-flavor
file=$ARCHIVE_LOCATION/minoca-bin-$ARCH$VARIANT$DEBUG.tar.gz
tar -cz -f $file ./bin
echo "Completed packaging $file"

