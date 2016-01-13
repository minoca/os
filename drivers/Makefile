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
       usrinput  \
       i8042     \
       null      \
       part      \
       plat      \
       pci       \
       ramdisk   \
       sd        \
       special   \
       term      \
       usb       \
       net       \
       videocon  \

include $(SRCROOT)/os/minoca.mk

i8042 usb: usrinput
ata usb: part

