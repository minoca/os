################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       EDMA3
#
#   Abstract:
#
#       This module implements support for the TI EDMA 3 controller.
#
#   Author:
#
#       Evan Green 1-Feb-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = edma3.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = edma3.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/dma.drv             \

include $(SRCROOT)/os/minoca.mk

