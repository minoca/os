################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       USB Composite
#
#   Abstract:
#
#       This module implements the USB Composite device driver, which exposes
#       child devices for each independent interface of a USB device.
#
#   Author:
#
#       Evan Green 20-Mar-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbcomp.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = usbcomp.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

