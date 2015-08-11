#! /bin/sh
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_veyron.sh
##
## Abstract:
##
##     This script creates the veyron.img image, a bootable SD card image for
##     the ASUS C201 (Veyron).
##
## Author:
##
##     Chris Stevens 6-Jul-2015
##
## Environment:
##
##     Build
##

set -e

OUTPUT=veyron.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

DRIVERS="$DRIVERS
dwhci.drv
sdrk32xx.drv
smsc95xx.drv"

BOOT_DRIVERS="$BOOT_DRIVERS
sdrk32xx.drv"

. $TASKDIR/images/assemble_common.sh
. $TASKDIR/images/assemble_efi.sh

mkdir -p $SYSTEM_DIRECTORY/veyron
cp -p veyronfw $SYSTEM_DIRECTORY/veyron/veyronfw

FIRMWARE_PARTITION_GUID="{fe3a2a5d-4f32-41a7-b725-accc3285a309}"
ATTRIBUTES=0x01FF000000000000
FIRMWARE_PARTITION_FORMAT=p:1M:"$FIRMWARE_PARTITION_GUID":"$ATTRIBUTES"

createimage -p "$FIRMWARE_PARTITION_FORMAT",p:10M*:e,p: -b2 -n3 -gciE \
    $CREATEIMAGE_DEBUG -f flat -o "$OUTPUT" -r 1,veyronfw -y EFI $IMAGE_FILES

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

