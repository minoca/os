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
#       Hardware Layer (32-bit Boot)
#
#   Abstract:
#
#       This directory builds a portion of the Hardware Layer Library for use
#       in the boot environment.
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

BINARY = hlboot.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/..;

OBJS = hmodapi.o     \
       dbgdev.o      \

X86_OBJS = x86/archdbg.o  \
           x86/ioport.o   \
           x86/regacces.o \
           ns16550.o      \

X64_OBJS := $(X86_OBJS)

VPATH += $(SRCDIR)/..:$(SRCDIR)/../..:

EXTRA_CPPFLAGS += -m32

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk

