################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       SMSC 91C111
#
#   Abstract:
#
#       This module implements support for the SMSC91C111 ethernet controller.
#
#   Author:
#
#       Chris Stevens 16-Apr-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = smsc91c1.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = sm91c1.o    \
       sm91c1hw.o  \

INCLUDES += $(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

