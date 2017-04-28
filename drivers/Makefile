################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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
       ahci      \
       ata       \
       devrem    \
       dma       \
       fat       \
       gpio      \
       input     \
       net       \
       null      \
       part      \
       pci       \
       plat      \
       ramdisk   \
       sd        \
       spb       \
       sound     \
       special   \
       term      \
       usb       \
       videocon  \

include $(SRCROOT)/os/minoca.mk

usb: input
ata usb: part
net: usb
plat: input spb

