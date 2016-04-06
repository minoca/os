################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       RK808
#
#   Abstract:
#
#       This module is the driver for the RK808 Power Management IC used in
#       platforms like the ASUS C201 Chromebook (Veyron Speedy).
#
#   Author:
#
#       Evan Green 4-Apr-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk808.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = rk808.o   \

DYNLIBS = $(BINROOT)/kernel              \

include $(SRCROOT)/os/minoca.mk

