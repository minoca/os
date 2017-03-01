################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#   Module Name:
#
#       SD
#
#   Abstract:
#
#       This module implements the Secure Digital host controller driver. It
#       can be used as a standalone driver or imported as a support library.
#
#   Author:
#
#       Evan Green 27-Feb-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sd.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = sd.o         \
       sdlib.o      \
       sdstd.o      \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

