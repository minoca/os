##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     add_common.sh
##
## Abstract:
##
##     This script adds files common to all target images on all architectures.
##     It is meant to be sourced from within another script, not run directly.
##
## Author:
##
##     Evan Green 9-Sep-2014
##
## Environment:
##
##     Build
##

DRIVERS=\
"acpi.drv
dwceth.drv
ehci.drv
fat.drv
netcore.drv
null.drv
onering.drv
part.drv
pci.drv
ser16550.drv
sd.drv
special.drv
usbcomp.drv
usbcore.drv
usbhub.drv
usbkbd.drv
usbmass.drv
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
loadefi
bootmefi.efi
libminocaos.so.1"

IMAGE_FILES=\
"minoca
apps"

LIB_FILES=\
"libc.so.1
libcrypt.so.1"

BIN_FILES=\
"debug
efiboot
mount
umount
msetup
profile
swiss
vmstat"

if test -z "$TASKDIR"; then
    echo "Error: TASKDIR was not defined."
    exit 1
fi

. $TASKDIR/images/add_common_$ARCH.sh

