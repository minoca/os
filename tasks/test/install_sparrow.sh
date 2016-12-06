#!/bin/sh
##
## Script Name:
##
##     install_sparrow.sh
##
## Abstract:
##
##     This script installs Sparrow tool on running Minoca OS.
##     Installation script takes several steps:
##     1) Installs sparrow system prerequisites (bash and curl), 
##     some system prerequisites ( like make, gcc so on ) are to be installed _earlier_
##     see os/tasks/build/add_files.sh file
##     2) Installs cpanminus client to install CPAN modules
##     3) Install sparrow tool as CPAN module
##
## Author:
##
##     Alexey Melezhik 06-Dec-2016
##
##

set -e
set -x

opkg -f /etc/opkg/opkg.conf update
opkg -f /etc/opkg/opkg.conf  install bash curl
curl -k -L https://cpanmin.us -o /usr/bin/cpan
chmod +x /usr/bin/cpanm
ln -fs /bin/env /usr/bin/env
cpanm --notest -q Sparrow
sparrow index update
