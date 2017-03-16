################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       USB HID
#
#   Abstract:
#
#       This module implements the USB HID parser library.
#
#   Author:
#
#       Evan Green 14-Mar-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbhid.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = hiddrv.o   \
       usbhid.o   \

DYNLIBS = $(BINROOT)/kernel                 \

include $(SRCROOT)/os/minoca.mk

