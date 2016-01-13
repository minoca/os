#!/bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     package_source.sh
##
## Abstract:
##
##     This script compresses the bin directory of the build output. SRCROOT,
##     ARCH, and DEBUG must all be set.
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

export TMPDIR=$PWD
export TEMP=$TMPDIR
export PATH="$SRCROOT/tools/win32/mingw/bin;$SRCROOT/tools;$SRCROOT/$ARCH$DEBUG/bin;$SRCROOT/$ARCH$DEBUG/testbin;$SRCROOT/$ARCH$DEBUG/bin/tools/bin;$SRCROOT/tools/win32/scripts;$SRCROOT/tools/win32/swiss;$SRCROOT/tools/win32/bin;$SRCROOT/tools/win32/ppython/app;$SRCROOT/tools/win32/ppython/App/Scripts;C:/Program Files/SlikSvn/bin;"
rm -rf ./src
mkdir ./src
echo "Exporting OS"
svn export --quiet $SRCROOT/os ./src/os
echo `svnversion $SRCROOT/os | sed 's/\([0-9]*\).*/\1/'` > ./src/os/revision
echo "Exporting third-party"
svn export --quiet $SRCROOT/third-party ./src/third-party
echo "Exporting tools"
mkdir -p ./src/tools
svn export --quiet $SRCROOT/tools/resources ./src/tools/resources
echo "Exporting client script"
svn export https://svn.freshkernel.com/svn/web/trunk/mweb/mbuild/client.py ./src/client.py
echo "Archiving"
set +e
tar -czf minoca-src.tar.gz ./src
echo "Done Packaging Source"

