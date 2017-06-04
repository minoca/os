################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       KD USB
#
#   Abstract:
#
#       This library contains the USB kernel debugger support library.
#
#   Author:
#
#       Evan Green 18-Apr-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = kdusb.a

BINARYTYPE = klibrary

INCLUDES += $(SRCROOT)/os/drivers/usb/ehci;

OBJS = ftdi.o   \
       hub.o    \
       kdehci.o \
       kdusb.o  \

DIRS = kdnousb

include $(SRCROOT)/os/minoca.mk

