################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       TestRtl
#
#   Abstract:
#
#       This program compiles the Rtl Library into an application and tests it.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = testrtl

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/build/basertl.a     \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a         \

OBJS = fptest.o   \
       heaptest.o \
       testrtl.o  \
       timetest.o \

include $(SRCROOT)/os/minoca.mk

