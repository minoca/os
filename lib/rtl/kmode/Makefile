################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Kernel Rtl
#
#   Abstract:
#
#       This library contains kernel-specific functions for the Runtime Library.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = krtl.a

BINARYTYPE = klibrary

INCLUDES += $(SRCDIR)/..;

OBJS = assert.o  \
       kprint.o  \
       pdouble.o \

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

