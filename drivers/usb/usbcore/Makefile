################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       USB Core
#
#   Abstract:
#
#       This module implements the USB core. It manages host controllers,
#       enumerates devices, manages transfers, and generally provides the high
#       level glue needed to make USB work.
#
#   Author:
#
#       Evan Green 15-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbcore.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = enum.o     \
       hub.o      \
       usbcore.o  \
       usbhost.o  \

INCLUDES += $(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

