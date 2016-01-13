##
## Copyright (c) 2014 Minoca Corp. All Rights Reserved.
##
## Script Name:
##
##     add_common_x86.sh
##
## Abstract:
##
##     This script adds files common to all x86 images.
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
"$DRIVERS
ata.drv
atl1c.drv
e100.drv
i8042.drv
rtl81xx.drv
uhci.drv"

BOOT_DRIVERS=\
"$BOOT_DRIVERS
ata.drv
pci.drv
ehci.drv
usbcomp.drv
usbhub.drv
usbmass.drv
sd.drv"

PCAT_FILES=\
"$PCAT_FILES
mbr.bin
fatboot.bin
bootman.bin
loader"

