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
##     download_build.sh
##
## Abstract:
##
##     This script downloads and extracts the latest build.
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
SCHEDULE_ARCH="$ARCH$VARIANT"
last_native_build=`python ../../client.py --query "Native Pilot $SCHEDULE_ARCH"`
if test -z $last_native_build; then
  echo "Error: Failed to get last Native Pilot $SCHEDULE_ARCH build."
  exit 1
fi

file=minoca-bin-$ARCH$VARIANT$DEBUG.tar.gz
echo "Downloading $file from schedule instance ID $last_native_build"
python ../../client.py --pull schedule $file $file $last_native_build
echo "Extracting $file"
tar -xzf $file
rm $file

