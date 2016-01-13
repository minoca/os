#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_bbone.sh
##
## Abstract:
##
##     This script creates the bbone.img image, a bootable SD card image for
##     the TI BeagleBone Black.
##
## Author:
##
##     Evan Green 19-Dec-2014
##
## Environment:
##
##     Build
##

set -e

OUTPUT=bbone.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

DRIVERS="$DRIVERS
sdomap4.drv
am3eth.drv"

BOOT_DRIVERS="$BOOT_DRIVERS
sdomap4.drv"

. $TASKDIR/images/assemble_common.sh
. $TASKDIR/images/assemble_efi.sh

mkdir -p $SYSTEM_DIRECTORY/bbone
cp -p bbonefw $SYSTEM_DIRECTORY/bbone/bbonefw
cp -p bbonemlo $SYSTEM_DIRECTORY/bbone/bbonemlo

createimage -p p:10M*:e,p: -b1 -n2 -aciE $CREATEIMAGE_DEBUG -f flat \
    -o "$OUTPUT" $IMAGE_FILES -m bbonemlo -y bbonefw -y EFI

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

