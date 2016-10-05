################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Empty RAM Disk
#
#   Abstract:
#
#       This file contains the symbols for a RAM disk image, but with nothing
#       in them.
#
#   Author:
#
#       Evan Green 5-Oct-2016
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = emptyrd.a

BINARYTYPE = library

INCLUDES += $(SRCROOT)/os/uefi/include;.;

OBJS = emptyrd.o  \

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

