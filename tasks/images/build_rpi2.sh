#! /bin/sh
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_rpi2.sh
##
## Abstract:
##
##     This script creates the rpi2.img image, a bootable SD card image for
##     the Raspberry Pi 2.
##
## Author:
##
##     Chris Stevens 17-Mar-2015
##
## Environment:
##
##     Build
##

set -e

OUTPUT=rpi2.img

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

##
## The Raspberry Pi and Raspberry Pi 2 share the same config file. As a result,
## use the 'rpifw' name for the Raspberry Pi 2 firmware when copying it into
## the image's system directory and onto the boot partition.
##

$OBJCOPY rpi2fw.elf -O binary rpifw
RPI2_DIRECTORY="$SYSTEM_DIRECTORY/rpi2"
mkdir -p "$RPI2_DIRECTORY"
cp -p rpifw "$RPI2_DIRECTORY/rpifw"
cp -p $SRCROOT/os/kernel/config/rpi/config.txt "$RPI2_DIRECTORY/config.txt"
cp -p $SRCROOT/os/kernel/config/rpi/start.elf "$RPI2_DIRECTORY/start.elf"
cp -p $SRCROOT/os/kernel/config/rpi/fixup.dat "$RPI2_DIRECTORY/fixup.dat"
cp -p $SRCROOT/os/kernel/config/rpi/bootcode.bin "$RPI2_DIRECTORY/bootcode.bin"

createimage -p p:20M*:f,p: -S -b1 -n2 -aciE $CREATEIMAGE_DEBUG -f flat \
    -o "$OUTPUT" $IMAGE_FILES -y rpifw -y EFI \
    -y $SRCROOT/os/kernel/config/rpi/config.txt \
    -y $SRCROOT/os/kernel/config/rpi/start.elf \
    -y $SRCROOT/os/kernel/config/rpi/fixup.dat \
    -y $SRCROOT/os/kernel/config/rpi/bootcode.bin

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

