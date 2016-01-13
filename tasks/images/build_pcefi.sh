#! /bin/sh
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     build_pcefi.sh
##
## Abstract:
##
##     This script creates the pcefi.img image.
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

OUTPUT=pcefi.img

if test -z "$TASKDIR"; then
    TASKDIR="$SRCROOT/os/tasks"
fi

. $TASKDIR/images/add_common.sh

##
## Include the Quark platform drivers.
##

if test -n "$QUARK"; then
    DRIVERS="$DRIVERS
qrkhostb.drv"
    BOOT_DRIVERS="$BOOT_DRIVERS
qrkhostb.drv"

fi

. $TASKDIR/images/assemble_common.sh
. $TASKDIR/images/assemble_efi.sh

##
## Quark builds a little differently. The second debug device is used rather
## than the first, and the terminal is different too.
##

if test -n "$QUARK"; then
    if test -n "$CREATEIMAGE_DEBUG"; then
        CREATEIMAGE_DEBUG="-D1"
        echo "Using UART1 ($CREATEIMAGE_DEBUG) for debug device on Intel Quark."
    fi

    echo "Using terminal 2 for Galileo."
    KERNEL_CMD='-k ps.env=CONSOLE=/dev/tty/Slave2'
fi

createimage -gp p:10M:e,p: -n2 -b1 -aci $CREATEIMAGE_DEBUG $KERNEL_CMD -f vmdk \
    -o "$OUTPUT" $IMAGE_FILES -y EFI

echo "Created $OUTPUT"
rm -rf $ROOT_DIRECTORY EFI

