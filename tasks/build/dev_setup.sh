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
##     dev_setup.sh
##
## Abstract:
##
##     This script sets up a development build environment with the necessary
##     packages to run the Minoca build client. When a subsequent OS image is
##     built, the developer can do "cd /auto; python client.py -v" in the
##     built OS to connect to the build server.
##
## Author:
##
##     Evan Green 24-Mar-2015
##
## Environment:
##
##     Build
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
BINROOT="$SRCROOT/$ARCH$VARIANT$DEBUG/bin"
APPSROOT="$BINROOT/apps"
AUTOROOT="$APPSROOT/auto"
PKGROOT="$BINROOT/packages"

##
## Note that the ghetto extractor doesn't extract package dependencies, so
## truly everything needed should be listed here.
##

PACKAGES="opkg
awk
binutils
byacc
flex
gcc
libgcc
gzip
m4
make
nano
openssh
patch
perl
expat
python2
tar
wget
libz
acpica
bzip2
sqlite
libiconv
libncurses
libreadline
libopenssl
libpcre
ca-certificates"

if ! [ -d $PKGROOT ]; then
    echo "*** Package directory $PKGROOT is missing. Please populate it. ***"
    exit 1
fi

mkdir -p "$AUTOROOT/"
cd $PKGROOT
missing=
found=

##
## Loop extracting packages.
##

for pkg in $PACKAGES; do

    ##
    ## Extract every package that matches package*.ipk.
    ##

    for ipk in ./$pkg*.ipk; do
        if [ ! -r $ipk ]; then
            missing="$missing $pkg"

        else
            found="$found $ipk"
            echo "=== Extracting $ipk ==="
            sh $SRCROOT/third-party/build/opkg-utils/opkg-extract-data \
                "$ipk" "$APPSROOT"
        fi
    done
done

if [ -n "$missing" ]; then
    if [ -z "$found" ]; then
        echo "*** No packages were found in $PKGROOT ***"

    else
        echo "*** The following required packages were missing from $PKGROOT:"
        echo "*** $missing"
        echo "*** Please go download them from the build server or build "
        echo "*** them manually."
    fi
fi

##
## Add client.py and the tasks directory.
##

git archive --format=tar --remote=ssh://git@git.minoca.co:2222/minoca/web.git \
    HEAD mweb/mbuild/client.py > $AUTOROOT/client.tar

tar -Oxf $AUTOROOT/client.tar > $AUTOROOT/client.py
rm $AUTOROOT/client.tar

cp -Rp "$SRCROOT/os/tasks" "$AUTOROOT/"
echo Completed adding files
exit 0

