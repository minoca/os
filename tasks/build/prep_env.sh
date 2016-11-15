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
##     prep_env.sh
##
## Abstract:
##
##     This script prepares the build environment.
##
## Author:
##
##     Evan Green 18-Jun-2014
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

export TMPDIR=$PWD
export TEMP=$TMPDIR

echo PATH=$PATH
echo -n Current Directory is
pwd
set
SAVED_IFS="$IFS"
IFS=':'

##
## Find the bin root.
##

BINROOT=
for path in $PATH; do
    if test -x $path/ld; then
        BINROOT=$path
        break
    fi
done

if test -z $BINROOT; then
    echo "Failed to find BINROOT."
    exit 1
fi

##
## Find swiss too.
##

SWISSPATH=
for path in $PATH; do
    if test -x $path/swiss; then
        SWISSPATH=$path
        break
    fi
done

if test -z $SWISSPATH; then
    echo "Failed to find SWISSPATH."
    exit 1
fi

IFS="$SAVED_IFS"

##
## Link the binutils apps to their full names as the OS makefile invokes them
## that way.
##

if test $ARCH = x86; then
    TRIO=i686-pc-minoca
    if test "x$VARIANT" = "xq"; then
        TRIO=i586-pc-minoca
    fi

elif test $ARCH = armv7; then
    TRIO=arm-none-minoca

elif test $ARCH = armv6; then
    TRIO=arm-none-minoca

else
    echo Unknown arch $ARCH.
    exit 1
fi

for app in ar as ld objcopy strip ranlib; do
  if ! test -x $BINROOT/$TRIO-$app; then
    ln -s $BINROOT/$app $BINROOT/$TRIO-$app
  fi

done

##
## Link the swiss binaries as parts of third-party need to find them directly.
##

for app in `$SWISSPATH/swiss --list`; do
  if ! test -x $BINROOT/$app; then
    ln -s $SWISSPATH/swiss $BINROOT/$app
  fi
done

