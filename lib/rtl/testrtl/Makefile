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

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a     \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a        \

OBJS = fptest.o   \
       heaptest.o \
       testrtl.o  \
       timetest.o \

include $(SRCROOT)/os/minoca.mk

