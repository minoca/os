################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       One Ring
#
#   Abstract:
#
#       This module implements the USB LED and USB Relay devices from One Ring
#       Road.
#
#   Author:
#
#       Evan Green 15-Jul-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = onering.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = onering.o   \

INCLUDES += $(SRCROOT)/os/drivers/usb/usbcore/inc; \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

DIRS = usbrelay \

include $(SRCROOT)/os/minoca.mk

