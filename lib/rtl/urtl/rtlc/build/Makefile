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
#       RTL C (Build)
#
#   Abstract:
#
#       This library contains stub functions to allow the Rtl Library to
#       compile in a standard C library on the build machine.
#
#   Author:
#
#       Evan Green 23-Oct-2012
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = rtlc.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/../..;$(SRCDIR)/../../..;

VPATH += $(SRCDIR)/..:$(SRCDIR)/../..:

OBJS = pdouble.o \
       stubs.o   \

include $(SRCROOT)/os/minoca.mk

