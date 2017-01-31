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
##     add_files.sh
##
## Abstract:
##
##     This script adds any additional files before the automation build image
##     is created.
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
AUTOROOT=$BINROOT/apps/auto

##
## Move the created distribution images aside.
##

if ! test -d "$BINROOT/distribute"; then
    mkdir "$BINROOT/distribute/"
    for file in $BINROOT/*.img $BINROOT/*.vmdk ; do
        if test -r "$file"; then
            mv $file "$BINROOT/distribute/"
        fi
    done
fi

##
## Add the client script and tasks, which are needed to fetch the *next*
## set of sources and binaries. Also add any additional test binaries to the
## images.
##

if ! test -d "$AUTOROOT"; then
    mkdir -p "$AUTOROOT"
    mkdir -p "$AUTOROOT/testbin"
    cp -v "$SRCROOT/client.py" "$AUTOROOT/client.py"
    cp -Rv "$SRCROOT/os/tasks/" "$AUTOROOT/"
    cp -v "$BINROOT/perftest" "$AUTOROOT/testbin/perftest"
    cp -v "$BINROOT/perflib.so" "$AUTOROOT/testbin/perflib.so"
fi

##
## Copy the skeleton over so the proper environment is set up for postinst
## scripts. Don't do this if it has already been done.
##

if ! [ -f $BINROOT/apps/etc/inittab ]; then
    cp -Rpf $BINROOT/skel/* $BINROOT/apps/ || true
fi

##
## Copy the script that automatically loads the Python build client.
## Manually symlink it in.
##

mkdir -p "$BINROOT/apps/etc/init.d/"
cp -v "$SRCROOT/os/tasks/build/autoclient.sh" "$BINROOT/apps/etc/init.d/"
chmod 0755 "$BINROOT/apps/etc/init.d/autoclient.sh"
for level in 2 3 4 5; do
    mkdir -p "$BINROOT/apps/etc/rc$level.d/"
    ln -sf "../init.d/autoclient.sh" \
        "$BINROOT/apps/etc/rc$level.d/S46autoclient.sh"

done

for level in 0 1 6; do
    mkdir -p "$BINROOT/apps/etc/rc$level.d/"
    ln -sf "../init.d/autoclient.sh" \
        "$BINROOT/apps/etc/rc$level.d/K64autoclient.sh"
done

##
## Create a local opkg configuration that prefers the local package repository.
##

package_dir="$BINROOT/packages"
cp /etc/opkg/opkg.conf ./myopkg.conf.orig
sed "s|src/gz main.*|src/gz local file:///$package_dir|" ./myopkg.conf.orig > \
    ./myopkg.conf

##
## Change Quark to i586.
##

if [ "$ARCH$VARIANT" = "x86q" ]; then
    sed "s|i686|i586|g" ./myopkg.conf > ./myopkg.conf2
    mv ./myopkg.conf2 ./myopkg.conf
fi

##
## Point the package repository at the build server.
##

DEST="$BINROOT/apps"
sed 's/www\.minocacorp\.com/10.0.1.202/' $DEST/etc/opkg/opkg.conf > opkg.tmp
mv opkg.tmp $DEST/etc/opkg/opkg.conf

##
## Perform an offline install of packages needed for the build.
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
ca-certificates
pkgconfig
yasm"

mkdir -p "$DEST/usr/lib/opkg/"

opkg --conf=$PWD/myopkg.conf --offline-root="$DEST" update
opkg --conf=$PWD/myopkg.conf --offline-root="$DEST" --force-postinstall \
    install $PACKAGES

rm -rf "$DEST/var/opkg-lists"

##
## Hard-code in a root password so that the system can be SSHed to.
##

chpasswd --root="$DEST" <<_EOF
root:minoca
_EOF

echo Completed adding files

