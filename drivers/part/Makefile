################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Partition
#
#   Abstract:
#
#       This module implements the partition support driver.
#
#   Author:
#
#       Evan Green 30-Jan-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = part.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = part.o  \

DYNLIBS = $(BINROOT)/kernel             \

TARGETLIBS = $(OBJROOT)/os/lib/partlib/partlib.a    \

include $(SRCROOT)/os/minoca.mk

