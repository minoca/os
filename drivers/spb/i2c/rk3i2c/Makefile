################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       RockChip RK3xxx I2C Driver
#
#   Abstract:
#
#       This module implements the I2C controller driver for the RockChip
#       RK3xxx SoC.
#
#   Author:
#
#       Evan Green 1-Apr-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk3i2c.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = rk3i2c.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk

