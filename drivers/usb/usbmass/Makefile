################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       USB Mass Storage
#
#   Abstract:
#
#       This module implements the USB Mass Storage class interface driver.
#
#   Author:
#
#       Evan Green 28-Jan-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = usbmass.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = usbmass.o   \

INCLUDES += ..;$(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

