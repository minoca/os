#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_integ.sh
##
## Abstract:
##
##     This script creates the integ.img image, the image for the ARM
##     Integrator/CP board emulation of Qemu.
##
## Author:
##
##     Evan Green 9-Sep-2014
##
## Environment:
##
##     Build
##

set -e

OUTPUT=integ.img

if test -z "$SRCROOT"; then
    echo "Error: SRCROOT must be set."
    exit 1
fi

if test -z "$BINROOT"; then
    echo "Error: BINROOT must be set."
    exit 1
fi

make -C $SRCROOT/os/uefi/plat/integcp RAMDISK=$BINROOT/armefi.img \
    BINARY=$OUTPUT SRCDIR=$SRCROOT/os/uefi/plat/integcp

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

