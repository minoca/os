################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Boot Runtime Library (32-bit)
#
#   Abstract:
#
#       This directory builds a subset of the Runtime Library in 32-bit mode
#       for use in the boot environment.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = basertlb.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/../../..;

VPATH += $(SRCDIR)/../..:

include $(SRCDIR)/../../sources

EXTRA_CPPFLAGS += -m32
X64_OBJS := $(X86_OBJS)

##
## This must be non-lto because otherwise for reasons not fully understood the
## intrinsic functions (like __aeabi_uidiv and __divdi3) get optimized away
## and leave the linker with undefined references to them.
##

LTO_OPT = -fno-lto

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk

