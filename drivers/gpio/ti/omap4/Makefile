################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       OMAP4 GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the TI OMAP4
#       SoC in the kernel.
#
#   Author:
#
#       Evan Green 4-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = om4gpio.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = om4gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk

