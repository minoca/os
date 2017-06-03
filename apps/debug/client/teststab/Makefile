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
#       STAB Test
#
#   Abstract:
#
#       This program is used to test the symbol parser of the debug client.
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

BINARY = teststab

BINARYTYPE = build

BINPLACE = testbin

BUILD = yes

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a   \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a \
             $(OBJROOT)/os/lib/im/build/imu.a             \

VPATH += $(SRCDIR)/..:
INCLUDES += $(SRCROOT)/os/lib/im;

OBJS = teststab.o   \
       stabs.o      \
       coff.o       \
       elf.o        \
       symbols.o    \

include $(SRCROOT)/os/minoca.mk

