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
SAVE_IFS="$IFS"
IFS='
'

export SRCROOT=`echo $SRCROOT | sed 's_\\\\_/_g'`
IFS="$SAVE_IFS"
unset SAVE_IFS

if test -z $SRCROOT; then
    echo "SRCROOT must be set."
    exit 1
fi

if test -z $ARCH; then
    echo "ARCH must be set."
    exit 1
fi

if test -z $DEBUG; then
    echo "DEBUG must be set."
    exit 1
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR
SOURCE_DIRECTORY=$1
shift
if test -z $SOURCE_DIRECTORY; then
    echo "first argument must be source directory."
fi

OUTROOT="$SRCROOT/$ARCH$VARIANT$DEBUG"
export PATH="$SRCROOT/tools/win32/mingw/bin;$OUTROOT/tools/bin;$OUTROOT/testbin;$SRCROOT/tools/win32/scripts;$SRCROOT/tools/win32/swiss;$SRCROOT/tools/win32/bin;$SRCROOT/tools/win32/ppython/app;$SRCROOT/tools/win32/ppython/App/Scripts;$PATH"
cd $SOURCE_DIRECTORY
echo Making in $PWD
echo make "$@"
make "$@"
echo completed make "$@"

