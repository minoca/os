################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       RK32 SPI Controller
#
#   Abstract:
#
#       This directory contains the controller support for SPI used on the
#       RockChip RK3288.
#
#   Author:
#
#       Evan Green 14-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk32spi.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = rk32spi.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk

