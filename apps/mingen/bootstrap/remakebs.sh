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
##     remakebs.sh [touch_file]
##
## Abstract:
##
##     This script rebuilds the bootstrap Makefiles. It requires Mingen, and
##     the Makefiles it rebuilds are those used to build the initial Mingen.
##     HOW WAS MINGEN BUILT THE FIRST TIME?
##
## Author:
##
##     Evan Green 3-May-2017
##
## Environment:
##
##     Build
##

set -e

destdir=`dirname $0`
input=$destdir/../../..

##
## These Makefiles will not be able to compile any Minoca-targeted code, since
## they are not created per ARCH. Explicitly set the architecture-dependent
## tools so the Makefiles come out the same every time.
##

export CC=no-cc
export OBJCOPY=no-objcopy
export AR=no-ar
export STRIP=no-strip

##
## Each OS that builds mingen differently needs an entry here. Most *nix OSes
## are similar to Minoca.
##

for os in Windows Darwin Linux Minoca; do

    ##
    ## Build a Makefile for the given build OS that can build mingen. This
    ## created mingen will then be able to build the real OS Makefile. The
    ## --unanchored flag prevents the input and output directories from being
    ## filled out. Without it these generated files would change based on the
    ## caller's build directory.
    ##

    mingen --format=make -i$input -O$destdir/Makefile.$os --unanchored \
        --no-generator --build-os=$os,i686 apps/mingen:build_mingen

done

##
## Offer to touch a file in the build directory so that the build system can
## see that something was done.
##

if [ -n "$1" ]; then
    touch "$1"
fi

