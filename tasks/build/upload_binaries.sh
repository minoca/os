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
file_size=`ls -l $file_path | \
    sed -n 's/[^ ]* *[^ ]* *[^ ]* *[^ ]* *\([0123456789]*\).*/\1/p'`

python ../../client.py --result "Binary Size" integer "$file_size"
python ../../client.py --upload schedule $file_path $file
echo Uploaded file $file_path, size $file_size

##
## Upload the packages. VERSION is major and minor numbers (ie 0.1 of 0.1.888).
##

package_dir="$ARCHIVE_LOCATION/packages"
VERSION=`cat $package_dir/kernel-version | sed 's/\([0-9]*\)\.\([0-9]*\)\..*/\1.\2/'`
case "$ARCH" in
  x86)

    ##
    ## Quark uses i586, everything else is i686.
    ##

    if test "x$VARIANT" = "xq"; then
      parch=i586

    else
      parch=i686
    fi
    ;;

  armv6) parch=armv6 ;;
  armv7) parch=armv7 ;;
  *)
    echo "Invalid arch $ARCH."
    exit 1
esac

for file in `ls $package_dir`; do
    python ../../client.py --upload package $package_dir/$file $VERSION/$parch/main/$file
done

rm -rf "$file_path" "$package_dir"

