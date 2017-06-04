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
#       Kernel Debugger (32-bit boot)
#
#   Abstract:
#
#       This library builds a portion of the Kernel Debugger Library for use in
#       the 32-bit boot environment.
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

BINARY = kdboot.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/../..:

include $(SRCDIR)/../../sources

EXTRA_CPPFLAGS += -m32

X64_OBJS := $(X86_OBJS)

EXTRA_SRC_DIRS = x86

include $(SRCROOT)/os/minoca.mk

