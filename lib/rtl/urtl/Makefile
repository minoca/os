################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       User Mode Runtime
#
#   Abstract:
#
#       This library contains the user-mode specific stubs needed by the core
#       runtime library (the Rtl library).
#
#   Author:
#
#       Evan Green 20-Feb-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = urtl.a

BINARYTYPE = library

INCLUDES += $(SRCDIR)/..;

OBJS = assert.o  \
       pdouble.o \
       uprint.o  \

DIRS = rtlc

include $(SRCROOT)/os/minoca.mk

