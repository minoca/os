################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#   Module Name:
#
#       SD Rockchip 32xx
#
#   Abstract:
#
#       This module implements the Rockchip 32xx SD controller driver.
#
#   Author:
#
#       Chris Stevens 29-Jul-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sdrk32xx.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = sdrk32.o    \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/sd.drv             \

include $(SRCROOT)/os/minoca.mk

