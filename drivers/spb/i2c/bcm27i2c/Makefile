################################################################################
#
#   Copyright (c) 2017 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Broadcom 27xx I2C Driver
#
#   Abstract:
#
#       This module implements the I2C controller driver for the Broadcom 27xx
#       SoC.
#
#   Author:
#
#       Chris Stevens 18-Jan-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = bcm27i2c.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = bcm27i2c.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk

