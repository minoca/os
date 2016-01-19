################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Serial 16550
#
#   Abstract:
#
#       This module implements support for 16550-like UARTs.
#
#   Author:
#
#       Evan Green 21-Nov-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ser16550.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = ser16550.o    \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

