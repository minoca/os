################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       ElfConv
#
#   Abstract:
#
#       This module builds the ElfConv utility, which converts an ELF image
#       into a PE image. UEFI exclusively loads PE images, which is why the
#       conversion is necessary. With certain restrictions (such as the lack
#       of dynamic libraries) conversion is doable, though not a lot of fun.
#
#   Author:
#
#       Evan Green 10-Mar-2014
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = elfconv

BUILD = yes

BINPLACE = bin

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/inc;

OBJS = elfc32.o     \
       elfconv.o    \

include $(SRCROOT)/os/minoca.mk

