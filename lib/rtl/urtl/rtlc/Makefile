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
#       RTL C
#
#   Abstract:
#
#       This library contains stub functions to allow the Rtl Library to
#       run on top of a standard C library.
#
#   Author:
#
#       Evan Green 23-Oct-2012
#
#   Environment:
#
#       User
#
################################################################################

BINARY = rtlc.a

BINARYTYPE = library

INCLUDES += $(SRCDIR)/..;$(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

OBJS = pdouble.o \
       stubs.o   \

DIRS = build

include $(SRCROOT)/os/minoca.mk

