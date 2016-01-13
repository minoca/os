################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Special
#
#   Abstract:
#
#       This module implements a special file driver that implements devices
#       like null, zero, and full.
#
#   Author:
#
#       Evan Green 23-Sep-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = special.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = special.o    \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

