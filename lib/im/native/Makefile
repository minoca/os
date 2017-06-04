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
#       Image Library (Native)
#
#   Abstract:
#
#       This directory builds the image library with only support for native
#       ELF binaries.
#
#   Author:
#
#       Evan Green 26-May-2017
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = imn.a

BINARYTYPE = klibrary

VPATH += $(SRCDIR)/..:

OBJS = imnative.o   \
       elfcomm.o    \

##
## x86 and ARM get 32-bit ELF. AMD64 gets 64-bit ELF.
##

X86_OBJS := elf.o   \

X64_OBJS := elf64.o \

ARMV7_OBJS := $(X86_OBJS)
ARMV6_OBJS := $(ARMV7_OBJS)

include $(SRCROOT)/os/minoca.mk

