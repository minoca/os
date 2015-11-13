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

##
## Compiling in eabisfp.c (and its dependency softfp.c) isn't necessary, but
## until there's a need for soft float (ie Cortex-M) compile it in so that it
## gets exercise.
##

ARMV7_OBJS = armv7/aeabisfp.o      \
             armv7/intrinsa.o      \
             armv7/intrinsc.o      \
             armv7/rtlmem.o        \
             boot/armv7/rtlarch.o  \
             fp2int.o              \
             softfp.o              \

ARMV6_OBJS = $(ARMV7_OBJS)

EXTRA_SRC_DIRS = x86 x64 armv7 boot/armv7

include $(SRCROOT)/os/minoca.mk

