################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       DMA
#
#   Abstract:
#
#       This directory contains system DMA drivers.
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

DMA_DRIVERS = edma3     \

DIRS = core             \
       $(DMA_DRIVERS)   \

include $(SRCROOT)/os/minoca.mk

$(DMA_DRIVERS): core

