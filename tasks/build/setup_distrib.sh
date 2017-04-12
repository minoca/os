#!/bin/sh
## Copyright (c) 2015 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information..
##
## Script Name:
##
##     setup_distrib.sh
##
## Abstract:
##
##     This script adds any additional files before the distribution build
##     image is created.
##
## Author:
##
##     Evan Green 20-Feb-2015
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
APPSROOT=$SRCROOT/$ARCH$VARIANT$DEBUG/bin/apps/

##
## Extract a few packages directly into the distribution image.
##

mkdir -p "$APPSROOT"

##
## Create a package index.
##

make_index="$SRCROOT/third-party/build/opkg-utils/opkg-make-index"
package_dir="$SRCROOT/$ARCH$VARIANT$DEBUG/bin/packages"
index_file="$package_dir/Packages"
python "$make_index" "$package_dir" > "$index_file"
cat "$index_file" | tr -d '\r' | gzip -9 > "${index_file}.gz"

##
## Create a local configuration that prefers the local package repository.
##

sed "s|src/gz main.*|src/gz local file:///$package_dir|" /etc/opkg/opkg.conf > \
    ./myopkg.conf

##
## Quark is i586 rather than i686.
##

if [ "$ARCH$VARIANT" = "x86q" ]; then
    sed "s|i686|i586|g" ./myopkg.conf > ./myopkg.conf2
    mv ./myopkg.conf2 ./myopkg.conf
fi

##
## Perform an offline install of the minimal set of packages.
##

PACKAGES="opkg gzip tar wget nano libpcre"
mkdir -p "$APPSROOT/usr/lib/opkg/"

opkg --conf=$PWD/myopkg.conf --offline-root="$APPSROOT" update
opkg --conf=$PWD/myopkg.conf --offline-root="$APPSROOT" --force-postinstall \
    install $PACKAGES

rm -rf "$APPSROOT/var/opkg-lists"
echo Completed adding files

