#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_panda.sh
##
## Abstract:
##
##     This script creates the panda.img image, a bootable SD card image for
##     the TI PandaBoard.
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

OUTPUT=panda.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

DRIVERS="$DRIVERS
sdomap4.drv
smsc95xx.drv"

BOOT_DRIVERS="$BOOT_DRIVERS
sdomap4.drv"

. $TASKDIR/images/assemble_common.sh
. $TASKDIR/images/assemble_efi.sh

mkdir -p $SYSTEM_DIRECTORY/panda
cp -p pandafw $SYSTEM_DIRECTORY/panda/pandafw
cp -p omap4mlo $SYSTEM_DIRECTORY/panda/omap4mlo

createimage -p p:10M*:e,p: -b1 -n2 -aciE $CREATEIMAGE_DEBUG -f flat \
    -o "$OUTPUT" $IMAGE_FILES -m omap4mlo -y pandafw -y EFI

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

