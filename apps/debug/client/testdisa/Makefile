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
#       Disassembler Test
#
#   Abstract:
#
#       This program is used to test the debugger's disassembler.
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

BINARY = testdisa

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a   \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a \
             $(OBJROOT)/os/lib/im/build/imu.a             \

VPATH += $(SRCDIR)/..:

OBJS = testdisa.o   \
       x86dis.o     \
       armdis.o     \
       disasm.o     \
       thmdis.o     \
       thm32dis.o   \

X86_OBJS = testdata.o \

include $(SRCROOT)/os/minoca.mk

