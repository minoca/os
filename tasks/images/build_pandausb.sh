#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_pandausb.sh
##
## Abstract:
##
##     This script creates the pandausb.img image, a bootable USB image for the
##     TI PandaBoard.
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

OUTPUT=pandausb.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

if test -z "$SRCROOT"; then
    echo "Error: SRCROOT must be set."
    exit 1
fi

if test -z "$BINROOT"; then
    echo "Error: BINROOT must be set."
    exit 1
fi

make -C $SRCROOT/os/uefi/plat/panda RAMDISK=$BINROOT/armefi.img \
    BINARY=pandafw_usb.elf SRCDIR=$SRCROOT/os/uefi/plat/panda

cp -p pandafw_usb "$OUTPUT"
echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

