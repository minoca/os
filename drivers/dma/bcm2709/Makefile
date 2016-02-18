################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       BCM2709 DMA
#
#   Abstract:
#
#       This module implements support for the Broadcom 2709 DMA controller.
#
#   Author:
#
#       Chris Stevens 12-Feb-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = dmab2709.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = dmab2709.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/dma.drv             \

include $(SRCROOT)/os/minoca.mk

