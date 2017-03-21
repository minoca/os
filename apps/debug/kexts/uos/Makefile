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
#       Kernel Debugger Extensions (Build Machine)
#
#   Abstract:
#
#       This shared library contains the debugger extensions built for the
#       build machine.
#
#   Author:
#
#       Evan Green 20-Mar-2017
#
#   Environment:
#
#       POSIX
#
################################################################################

SO_VERSION_MAJOR := 1
SO_VERSION_MINOR := 0

BINARY := kexts.so

BINARYTYPE = so

BUILD = yes

BINPLACE = tools/lib

TARGETLIBS = $(TOOLROOT)/lib/dbgext.a

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

OS ?= $(shell uname -s)

OBJS = acpiext.o  \
       kexts.o    \
       memory.o   \
       objects.o  \
       reslist.o  \
       threads.o  \

ifeq ($(OS),Darwin)
BINARY := kexts.$(SO_VERSION_MAJOR).dylib
endif

include $(SRCROOT)/os/minoca.mk
