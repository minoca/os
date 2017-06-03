################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       DWARF Test
#
#   Abstract:
#
#       This program is used to test the DWARF symbol parser in isolation.
#
#   Author:
#
#       Evan Green 2-Dec-2015
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = tdwarf

BINARYTYPE = build

BINPLACE = testbin

BUILD = yes

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a   \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a \
             $(OBJROOT)/os/lib/im/build/imu.a             \

VPATH += $(SRCDIR)/..:
INCLUDES += $(SRCROOT)/os/lib/im;

OBJS = coff.o        \
       elf.o         \
       dwarf.o       \
       dwexpr.o      \
       dwframe.o     \
       dwline.o      \
       dwread.o      \
       stabs.o       \
       symbols.o     \
       tdwarf.o      \

include $(SRCROOT)/os/minoca.mk

