################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mkuboot
#
#   Abstract:
#
#       This module builds the mkuboot build utility, which can create a U-Boot
#       firmware images.
#
#   Author:
#
#       Chris Stevens 2-Jul-2015
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = mkuboot

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/include;

VPATH += $(SRCROOT)/os/uefi/core

OBJS = mkuboot.o \
       crc32.o   \

include $(SRCROOT)/os/minoca.mk

