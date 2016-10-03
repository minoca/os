################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       GenFV
#
#   Abstract:
#
#       This module builds the GenFV build utility, which can create a FFS2
#       Firmware Volume out of one or more FFS files.
#
#   Author:
#
#       Evan Green 7-Mar-2014
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = genfv

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/include;

OBJS = genfv.o

include $(SRCROOT)/os/minoca.mk

