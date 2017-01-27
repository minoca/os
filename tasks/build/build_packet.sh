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

##
## Exit if this is not x86.
##

if [ "$ARCH$VARIANT" != "x86" ]; then
    echo "Skipping Packet build on $ARCH$VARIANT."
fi

export TMPDIR=$PWD
export TEMP=$TMPDIR
BINROOT="$SRCROOT/$ARCH$VARIANT$DEBUG/bin"
DEST="$BINROOT/apps"

##
## Move the install image aside temporarily.
##

mv install.img install_.img

##
## Copy the skeleton over so the proper environment is set up for postinst
## scripts.
##

cp -Rp $BINROOT/skel/* $BINROOT/apps/

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

##
## Hard-code in a root password so that the system can be SSHed to.
## TODO: Remove this once everything's working.
##

chpasswd --root="$DEST" <<_EOF
root:minoca
_EOF

##
## Shove an SSH key in there too for testing.
## TODO: Remove this once everything's working.
##

mkdir -p "$DEST/root/.ssh"
cat >"$DEST/root/.ssh/authorized_keys" <<_EOF
ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAACAQCvSrbnIgUQW/gZP1wDhjI4c+s0HnGjVzkdT5Kk2mKYBdCF0YZ4Du6FUtHVR9EFh696piC3Qx0FKk4YddE1qotv6GxOJWo7rZdWt4Nm8i5mgV4MX9dhZ/SxYJGFKfBucyhIDO7ZCP61WiNPTw7lB4bt9Nl6qJlCTDTnlBsn+6XcJboJyjj6TWpHOtwi/8VItTiTxQ7M6A4aHwgePRixcsBo8VYFyYP8cl0p/N3BIPTRtkY+vqcZIkE0kZKbMgI/U3L4vWOJ5/NzWKY428YrY2wCKQF7QBsvqR/z9kM5TWYoC6w1rm9bbvr/Y4MmxqQlDrMifU18vwDMaU/wy8yskDSKRlw87Oz1dTxmmm1uSDeAjBTrb1nI1C6jGgKYucgXnv03OXYncQbBzvIrJl8XbE9REzFDC5k3GyxChIXMlyze2EqJcMyqHMyLZLQXfl0i0cvE76BqlCGDMWum19L/gtoZxmMNdeuYk00mqyoP16o2SeA2upaR9/VOo+Wel7D0D/uELC7zFySUeVzBofRCiZ6Kg7JceoqZ11thVgQX0305byBqFT9lFGmdT3CFAMMw8gUKAUJqolVCmiITUuQdL6iWakgqEEp0+Yfiz5VLcwsDIcyUHjTuHAgKQ0deHI9hS/d+Z9xKnZUnWTtEDJT3+brg2v9UE2ThXif+io3DiXXNvQ== Evan Packet
_EOF

chmod 600 "$DEST/root/.ssh/authorized_keys"
chmod 700 "$DEST/root/.ssh"

##
## Set up the cloud.cfg file.
##

cat > "$DEST/etc/cloud.cfg" <<"_EOF"
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

echo Done building Packet image.

