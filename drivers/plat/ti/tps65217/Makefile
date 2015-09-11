################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       TPS65217
#
#   Abstract:
#
#       This module is the driver for the TPS65217 Power Management IC used in
#       platforms like the BeagleBone Black.
#
#   Author:
#
#       Evan Green 8-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = tps65217.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = tps65217.o   \

DYNLIBS = $(BINROOT)/kernel              \

include $(SRCROOT)/os/minoca.mk

