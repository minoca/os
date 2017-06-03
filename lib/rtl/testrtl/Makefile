################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       TestRtl
#
#   Abstract:
#
#       This program compiles the Rtl Library into an application and tests it.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = testrtl

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a     \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a   \

OBJS = fpstest.o  \
       fptest.o   \
       heaptest.o \
       testrtl.o  \
       timetest.o \

include $(SRCROOT)/os/minoca.mk

