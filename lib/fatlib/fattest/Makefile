################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       FAT Test
#
#   Abstract:
#
#       This program tests the FAT file system library.
#
#   Author:
#
#       Evan Green 9-Oct-2012
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = fattest

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a      \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a         \
             $(OBJROOT)/os/lib/fatlib/build/fat.a            \

OBJS = fattest.o \
       fatdev.o  \

include $(SRCROOT)/os/minoca.mk

