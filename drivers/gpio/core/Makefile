################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       GPIO Core
#
#   Abstract:
#
#       This module implements the GPIO core support library. It provides
#       generic support infrastructure for all GPIO drivers.
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

BINARY = gpio.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = gpio.o     \

DYNLIBS = $(BINROOT)/kernel \

include $(SRCROOT)/os/minoca.mk

