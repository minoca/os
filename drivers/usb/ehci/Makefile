################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       EHCI
#
#   Abstract:
#
#       This module implements the EHCI USB 2.0 Host Controller Driver.
#
#   Author:
#
#       Evan Green 18-Mar-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ehci.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = ehci.o     \
       ehcihc.o   \

INCLUDES += ..;$(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

