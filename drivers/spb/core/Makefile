################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Simple Peripheral Bus Core
#
#   Abstract:
#
#       This module implements the Simple Peripheral Bus core support library.
#       It provides generic support infrastructure for all SPB drivers, both
#       devices and controllers.
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

BINARY = spb.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = spb.o     \

DYNLIBS = $(BINROOT)/kernel \

include $(SRCROOT)/os/minoca.mk

