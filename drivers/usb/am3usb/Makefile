################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       TI AM33xx USB
#
#   Abstract:
#
#       This module implements the USB 2.0 OTG controller on TI AM33xx SoCs.
#
#   Author:
#
#       Evan Green 11-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3usb.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = am3usb.o       \
       am3usbhw.o     \
       cppi.o         \
       musb.o         \

INCLUDES += ..;$(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

