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
#       Kernel Rtl (32-bit)
#
#   Abstract:
#
#       This library contains kernel-specific functions for the Runtime Library.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = krtl.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/../..;
VPATH += $(SRCDIR)/..:

EXTRA_CPPFLAGS += -m32

OBJS = assert.o  \
       kprint.o  \
       pdouble.o \

include $(SRCROOT)/os/minoca.mk

