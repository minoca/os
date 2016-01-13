#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_rpi.sh
##
## Abstract:
##
##     This script creates the rpi.img image, a bootable SD card image for
##     the Raspberry Pi.
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

OUTPUT=rpi.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

DRIVERS="$DRIVERS
dwhci.drv
smsc95xx.drv
sdbm2709.drv"

BOOT_DRIVERS="$BOOT_DRIVERS
sdbm2709.drv"

. $TASKDIR/images/assemble_common.sh
. $TASKDIR/images/assemble_efi.sh

$OBJCOPY rpifw.elf -O binary rpifw
RPI_DIRECTORY="$SYSTEM_DIRECTORY/rpi"
mkdir -p "$RPI_DIRECTORY"
cp -p rpifw "$RPI_DIRECTORY/rpifw"
cp -p $SRCROOT/os/kernel/config/rpi/config.txt "$RPI_DIRECTORY/config.txt"
cp -p $SRCROOT/os/kernel/config/rpi/start.elf "$RPI_DIRECTORY/start.elf"
cp -p $SRCROOT/os/kernel/config/rpi/fixup.dat "$RPI_DIRECTORY/fixup.dat"
cp -p $SRCROOT/os/kernel/config/rpi/bootcode.bin "$RPI_DIRECTORY/bootcode.bin"

createimage -p p:20M*:f,p: -S -b1 -n2 -aciE $CREATEIMAGE_DEBUG -f flat \
    -o "$OUTPUT" $IMAGE_FILES -y rpifw -y EFI \
    -y $SRCROOT/os/kernel/config/rpi/config.txt \
    -y $SRCROOT/os/kernel/config/rpi/start.elf \
    -y $SRCROOT/os/kernel/config/rpi/fixup.dat \
    -y $SRCROOT/os/kernel/config/rpi/bootcode.bin

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

