#!/bin/sh
## Copyright (c) 2017 Minoca Corp.
##
##    This file is licensed under the terms of the GNU General Public License
##    version 3. Alternative licensing terms are available. Contact
##    info@minocacorp.com for details. See the LICENSE file at the root of this
##    project for complete licensing information.
##
## Script Name:
##
##     prep_auto.sh
##
## Abstract:
##
##     This script runs once during init on an automation machine, and sets up
##     the automation.
##
## Author:
##
##     Evan Green 29-Jun-2017
##
## Environment:
##
##     Minoca Build
##

exec >/var/log/prep_auto.log 2>&1
set -xe

##
## Set up the autoclient to run on init.
##

cp /auto/tasks/build/autoclient.sh /etc/init.d/autoclient.sh
chmod +x /etc/init.d/autoclient.sh
/usr/sbin/update-rc.d autoclient.sh defaults 46

##
## Point the package repository at the build server.
##

opkg_config=/etc/opkg/opkg.conf
sed 's/www\.minocacorp\.com/10.0.1.202/' $opkg_config > ${opkg_config}.tmp
mv "${opkg_config}.tmp" "$opkg_config"

##
## Install the needed packages.
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

opkg update
opkg install $PACKAGES

##
## Hard-code in a root password so that the test machine can be SSHed to.
## These machines must not be exposed on the Internet, since the password is
## stupid and obvious.
##

chpasswd <<_EOF
root:minoca
_EOF

##
## Start the auto client and SSH.
##

/etc/init.d/autoclient.sh start
/etc/init.d/sshd start

##
## Remove this file, since one-time initialization is complete.
##

rm -v /etc/rc2.d/S*prep_auto.sh

echo Done

