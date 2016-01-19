################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

INCLUDES += $(SRCROOT)/os/drivers/usb/usbcore/inc;
INCLUDES += $(SRCROOT)/os/drivers/usb/ehci;

OBJS = ftdi.o   \
       hub.o    \
       kdehci.o \
       kdusb.o  \

DIRS = kdnousb

include $(SRCROOT)/os/minoca.mk

