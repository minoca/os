################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#   Module Name:
#
#       SD OMAP4
#
#   Abstract:
#
#       This module implements the OMAP4 SD controller driver.
#
#   Author:
#
#       Evan Green 16-Mar-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sdomap4.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = i2c.o        \
       pmic.o       \
       sdomap4.o    \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/sd.drv             \

include $(SRCROOT)/os/minoca.mk

