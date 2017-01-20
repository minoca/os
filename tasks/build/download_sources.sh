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
##     download_sources.sh
##
## Abstract:
##
##     This script downloads and extracts the latest sources.
##
## Author:
##
##     Evan Green 15-May-2014
##
## Environment:
##
##     Minoca Build
##

set -e

if test -z "$SRCROOT"; then
    SRCROOT=`pwd`/src
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR
last_windows_build=`python ../../client.py --query "Windows Pilot"`
if test -z $last_windows_build; then
  echo "Error: Failed to get last Windows Pilot build."
  exit 1
fi

mkdir -p $SRCROOT
file=minoca-src.tar.gz
echo "Downloading $file from schedule instance ID $last_windows_build"
python ../../client.py --pull schedule $file $SRCROOT/../$file \
    $last_windows_build

echo "Extracting $file"
cd $SRCROOT/..
tar -xzf $file
echo "Done extracting $file"
chmod +x $SRCROOT/os/tasks/build/*.sh
chmod +x $SRCROOT/os/tasks/test/*.sh
rm $file
