#!/bin/sh
## Copyright (c) 2017 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     test_cleanup.sh
##
## Abstract:
##
##     This script removes the native build artifacts.
##
## Author:
##
##     Evan Green 28-Jun-2017
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
ARCHIVE_LOCATION=$PWD/..
file=minoca-bin-$ARCH$VARIANT$DEBUG.tar.gz
file_path=$ARCHIVE_LOCATION/$file
package_dir="$ARCHIVE_LOCATION/packages"

rm -v "$file_path"
rm -rfv "$package_dir"

