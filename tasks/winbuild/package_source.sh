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
OUTROOT=$SRCROOT/$ARCH$VARIANT$DEBUG
export PATH="$SRCROOT/tools/win32/mingw/bin;$OUTROOT/tools/bin;$OUTROOT/testbin;$SRCROOT/tools/win32/scripts;$SRCROOT/tools/win32/swiss;$SRCROOT/tools/win32/bin;$SRCROOT/tools/win32/ppython/app;$SRCROOT/tools/win32/ppython/App/Scripts;$PATH"
rm -rf ./src
mkdir ./src

echo "Exporting OS"
cd $SRCROOT/os
git archive HEAD > $TMPDIR/os.tar
mkdir $TMPDIR/src/os
tar -xf $TMPDIR/os.tar -C $TMPDIR/src/os
rm $TMPDIR/os.tar
mkdir $TMPDIR/src/os/.git
echo $((`git rev-list --count HEAD` + 1000)) > $TMPDIR/src/os/revision
git rev-parse --abbrev-ref HEAD > $TMPDIR/src/os/branch
git rev-parse HEAD > $TMPDIR/src/os/commit
git rev-parse HEAD > $TMPDIR/src/os/.git/HEAD

echo "Exporting third-party"
cd $SRCROOT/third-party
git archive HEAD > $TMPDIR/tp.tar
mkdir $TMPDIR/src/third-party
tar -xf $TMPDIR/tp.tar -C $TMPDIR/src/third-party
rm $TMPDIR/tp.tar

echo "Exporting client script"
cd $TMPDIR
git archive --format=tar --remote=ssh://git@git.minoca.co:2222/minoca/web.git \
    HEAD mweb/mbuild/client.py > web.tar

tar -Oxf web.tar > ./src/client.py
rm web.tar

echo "Archiving"
set +e
tar -czf minoca-src.tar.gz ./src
echo "Done Packaging Source"

