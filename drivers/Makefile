################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Drivers
#
#   Abstract:
#
#       This directory contains kernel-mode drivers that provide hardware
#       support for the kernel and applications.
#
#   Author:
#
#       Evan Green 16-Sep-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = acpi      \
       ata       \
       devrem    \
       fat       \
       gpio      \
       i8042     \
       net       \
       null      \
       part      \
       pci       \
       plat      \
       ramdisk   \
       sd        \
       spb       \
       special   \
       term      \
       usb       \
       usrinput  \
       videocon  \

include $(SRCROOT)/os/minoca.mk

i8042 usb: usrinput
ata usb: part
net: usb
plat: usrinput spb

