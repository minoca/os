#! /bin/sh
## Copyright (c) 2015 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_tinydemo.sh
##
## Abstract:
##
##     This script creates the tinydemo.img image, which contains the bare
##     minimum to run a keyboard, networking, and shell on Qemu.
##
## Author:
##
##     Evan Green 3-Aug-2015
##
## Environment:
##
##     Build
##

set -e

OUTPUT=tinydemo.img
APPS_DIR=appstiny

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

DRIVERS=\
"acpi.drv
fat.drv
netcore.drv
null.drv
part.drv
pci.drv
special.drv
usrinput.drv
videocon.drv"

BOOT_DRIVERS=\
"acpi.drv
fat.drv
null.drv
part.drv
special.drv
videocon.drv"

BOOT_DRIVERS_FILE="bootdrv.set"

CONFIG_FILES=\
"dev2drv.set
devmap.set
init.set
init.sh
tzdata
tzdflt"

SYSTEM_FILES=\
"initcon
kernel
libminocaos.so.1"

IMAGE_FILES=\
"minoca
$APPS_DIR"

LIB_FILES=" "

BIN_FILES=\
"swiss
mount"

DRIVERS=\
"$DRIVERS
ata.drv
e100.drv
i8042.drv"

BOOT_DRIVERS=\
"$BOOT_DRIVERS
ata.drv
pci.drv"

PCAT_FILES=\
"$PCAT_FILES
loader"

. $TASKDIR/images/assemble_common.sh

##
## Double check that APPS_DIR is set to avoid removing /bin/sh.
##

if test -z "$APPS_DIR"; then
    echo "APPS_DIR not set"
    exit 1
fi

##
## Get rid of a little bit of redundancy.
##

rm $APPS_DIR/bin/sh
rm $APPS_DIR/lib/libminocaos.so.1
$STRIP $APPS_DIR/lib/libc.so.1
$STRIP $APPS_DIR/lib/libcrypt.so.1

echo -n "$SYSTEM_DIRECTORY/libminocaos.so.1 --library-path $APPS_DIR/lib \
$APPS_DIR/bin/swiss sh $CONFIG_DIRECTORY/init.sh" >$CONFIG_DIRECTORY/init.set

createimage -p p:40M* -b1 -n1 -c $CREATEIMAGE_DEBUG -f vmdk \
    -o "$OUTPUT" -m mbr.bin -x fatboot.bin -y bootman.bin \
    -k 'ps.env=WORLD=appstiny' $IMAGE_FILES

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

