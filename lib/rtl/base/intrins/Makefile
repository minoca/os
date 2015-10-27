################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Intrinsics
#
#   Abstract:
#
#       This library contains functions in the runtime library emitted by the
#       compiler directly.
#
#   Author:
#
#       Evan Green 12-Nov-2014
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = intrins.a

BINARYTYPE = library

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

ARMV7_OBJS = armv7/intrinsa.o  \
             armv7/intrinsc.o  \

ARMV6_OBJS = $(ARMV7_OBJS)

X86_OBJS = x86/intrinsc.o  \

##
## The library is being compiled such that the rest of Rtl is in another binary.
##

EXTRA_CFLAGS += -DRTL_API=DLLIMPORT

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

