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
##     build_packet.sh
##
## Abstract:
##
##     This script builds the Packet image.
##
## Author:
##
##     Evan Green 26-Jan-2017
##
## Environment:
##
##     Minoca Build
##

if test -z "$SRCROOT"; then
    export SRCROOT=`pwd`/src
fi

if test -z "$ARCH"; then
    echo "ARCH must be set."
    exit 1
fi

if test -z "$DEBUG"; then
    echo "DEBUG must be set."
    exit 1
fi

##
## Exit if this is not x86.
##

if [ "$ARCH$VARIANT" != "x86" ]; then
    echo "Skipping Packet build on $ARCH$VARIANT."
    exit 0
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR
BINROOT="$SRCROOT/$ARCH$VARIANT$DEBUG/bin"
DEST="$BINROOT/apps"

cd $BINROOT

##
## Move the install image aside temporarily.
##

mv install.img install_.img

##
## Copy the skeleton over so the proper environment is set up for postinst
## scripts.
##

cp -Rpf $BINROOT/skel/* $BINROOT/apps/ || true

##
## Create a local opkg configuration that prefers the local package repository.
##

package_dir="$BINROOT/packages"
cp /etc/opkg/opkg.conf ./myopkg.conf.orig
sed "s|src/gz main.*|src/gz local file:///$package_dir|" ./myopkg.conf.orig > \
    ./myopkg.conf

##
## Install python-setuptools on the build machine.
##

opkg --conf=$PWD/myopkg.conf update
opkg --conf=$PWD/myopkg.conf install python-pip

##
## Perform an offline install of packages needed for the build.
##

PACKAGES="opkg
libgcc
gzip
nano
openssh
expat
python2
python-setuptools
tar
wget
sqlite
libiconv
libncurses
libreadline
libopenssl
libpcre
ca-certificates
python-cloud-init"

mkdir -p "$DEST/usr/lib/opkg/"

opkg --conf=$PWD/myopkg.conf --offline-root="$DEST" update
opkg --conf=$PWD/myopkg.conf --offline-root="$DEST" --force-postinstall \
    install $PACKAGES

rm -rf "$DEST/var/opkg-lists"

##
## Perform an offline install of the python packages. This can go away once
## all these packages (and their dependencies) are actually added as official
## packages, rather than being fetched from Pypi.
##

PYPACKAGES="cheetah
jinja2
PrettyTable
oauth
pyserial
configobj
pyyaml
argparse
requests
jsonpatch"

pip install --target="$DEST/usr/lib/python2.7/site-packages" $PYPACKAGES
mkdir -p "$DEST/root/.ssh"
chmod 700 "$DEST/root/.ssh"

##
## Set up the cloud.cfg file.
##

cat > "$DEST/etc/cloud/cloud.cfg" <<"_EOF"
disable_root: 0
ssh_pwauth:   0
datasource:
  Ec2:
    timeout: 10
    max_wait: 20
    metadata_urls: [ 'https://metadata.packet.net' ]
    dsmode: net
cloud_init_modules:
 - DataSourceEc2
 - migrator
 - bootcmd
 - write-files
 - growpart
# - resizefs
 - set_hostname
 - update_hostname
 - update_etc_hosts
 - rsyslog
 - ssh
cloud_config_modules:
 - DataSourceEc2
 - mounts
# - locale
 - set-passwords
 - yum-add-repo
 - package-update-upgrade-install
 - timezone
 - puppet
 - chef
 - salt-minion
 - mcollective
 - runcmd
cloud_final_modules:
 - DataSourceEc2
 - scripts-per-once
 - scripts-per-boot
 - scripts-per-instance
 - scripts-user
 - ssh-authkey-fingerprints
 - keys-to-console
 - phone-home
 - final-message

system_info:
   distro: gentoo # Pick gentoo because it uses /etc/init.d/ for init scripts.

_EOF

##
## Rebuild the install.img.
##

cd $SRCROOT/os/images
make install.img

##
## Rename the image, and put the old install.img back.
##

cd $BINROOT
mv install.img packet.img
mv install_.img install.img

##
## Remove cloud-init.
##

opkg --conf=$PWD/myopkg.conf --offline-root="$DEST" --force-postinstall \
    remove python-cloud-init

export SYSROOT="$DEST"
for step in cloud-init-local cloud-init cloud-init-config cloud-init-final;
do
    update-rc.d -f $step remove
    rm -f $SYSROOT/etc/init.d/$step
done

echo Done building Packet image.

