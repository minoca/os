################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       TI AM335x I2C Driver
#
#   Abstract:
#
#       This module implements the I2C controller driver for the TI AM335x SoC.
#
#   Author:
#
#       Evan Green 7-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3i2c.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = am3i2c.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/spb.drv             \

include $(SRCROOT)/os/minoca.mk

