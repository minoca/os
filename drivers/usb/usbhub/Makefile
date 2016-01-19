################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       USB Hub
#
#   Abstract:
#
#       This module implements the USB Hub class driver.
#
#   Author:
#
#       Evan Green 16-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbhub.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = usbhub.o   \

INCLUDES += ..;$(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

