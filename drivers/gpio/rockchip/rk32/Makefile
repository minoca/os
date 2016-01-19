################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       RK32xx GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the RockChip
#       RK32xx SoC.
#
#   Author:
#
#       Evan Green 25-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk32gpio.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = rk32gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk

