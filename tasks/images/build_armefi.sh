#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_armefi.sh
##
## Abstract:
##
##     This script creates the armefi.img image.
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

OUTPUT=armefi.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

DRIVERS="$DRIVERS
ramdisk.drv
pl050.drv
om4gpio.drv
sdomap4.drv
smsc91c1.drv
smsc95xx.drv"

BOOT_DRIVERS="$BOOT_DRIVERS
ramdisk.drv
om4gpio.drv
sdomap4.drv"

##
## The lib files and bin files need to get put in the root since apps is being
## excluded. If there is no apps, this would happen anyway.
##

IMAGE_FILES="minoca"
if test -d "apps/lib"; then
    IMAGE_FILES="$IMAGE_FILES $LIB_FILES"
fi

if test -d "apps/bin"; then
    IMAGE_FILES="$IMAGE_FILES $BIN_FILES"
fi

. $TASKDIR/images/assemble_common.sh

printf "libminocaos.so.1 --library-path . minoca/system/initcon \
 -e LD_LIBRARY_PATH=. swiss sh minoca/config/init.sh" > \
 minoca/config/init.set

. $TASKDIR/images/assemble_efi.sh

createimage -p p:10M*:e,p: -b1 -n2 -aciE $CREATEIMAGE_DEBUG -f flat \
    -o "$OUTPUT" $IMAGE_FILES -y EFI

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

