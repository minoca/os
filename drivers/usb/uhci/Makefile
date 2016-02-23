################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       UHCI
#
#   Abstract:
#
#       This module implements the UHCI USB 1.1 Host Controller Driver.
#
#   Author:
#
#       Evan Green 13-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = uhci.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = uhci.o     \
       uhcihc.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

