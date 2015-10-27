################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Boot Runtime Library
#
#   Abstract:
#
#       This directory builds a subset of the Runtime Library for use in the
#       boot environment.
#
#   Author:
#
#       Chris Stevens 18-Mar-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = basertlb.a

BINARYTYPE = library

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ARMV7_OBJS = armv7/rtlmem.o        \
             armv7/intrinsa.o      \
             armv7/intrinsc.o      \
             boot/armv7/rtlarch.o  \
             softfp.o              \

ARMV6_OBJS = $(ARMV7_OBJS)

EXTRA_SRC_DIRS = x86 x64 armv7 boot/armv7

include $(SRCROOT)/os/minoca.mk

