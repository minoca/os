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
##     provision_packet.sh
##
## Abstract:
##
##     This script runs on the Alpine Linux recovery OS and installs Minoca OS.
##     It assumes wget is there.
##
## Author:
##
##     Evan Green 3-Feb-2017
##
## Environment:
##
##     Packet Rescue OS (Alpine Linux running out of RAM).
##

set -xe

wget https://www.minocacorp.com/download/packet/packet.img
wget https://www.minocacorp.com/download/packet/msetup.alpine
chmod +x ./msetup.alpine
mv packet.img install.img
./msetup.alpine -d/dev/sda
: Everythings all set. Hit reboot to boot into Minoca OS.

