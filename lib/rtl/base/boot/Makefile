################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = klibrary

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

##
## This must be non-lto because otherwise for reasons not fully understood the
## intrinsic functions (like __aeabi_uidiv and __divdi3) get optimized away
## and leave the linker with undefined references to them.
##

LTO_OPT = -fno-lto

EXTRA_SRC_DIRS = x86 x64 armv7 boot/armv7

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

