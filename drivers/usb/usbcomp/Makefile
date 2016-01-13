################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINARYTYPE = so

BINPLACE = bin

OBJS = usbcomp.o   \

INCLUDES += $(SRCROOT)/os/drivers/usb/usbcore/inc; \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

