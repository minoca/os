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

BINPLACE = bin

BINARYTYPE = build

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a      \
             $(OBJROOT)/os/lib/rtl/build/rtlstubs.a          \

INCLUDES += $(SRCROOT)/os/uefi/inc;

OBJS = mkuboot.o

include $(SRCROOT)/os/minoca.mk

