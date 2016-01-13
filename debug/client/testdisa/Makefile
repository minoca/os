################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/build/basertl.a   \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a       \
             $(INTLIB)/os/lib/im/build/im.a              \

VPATH += $(SRCDIR)/..:

OBJS = testdisa.o   \
       x86dis.o     \
       armdis.o     \
       disasm.o     \
       thmdis.o     \
       thm32dis.o   \

X86_OBJS = testdata.o \

include $(SRCROOT)/os/minoca.mk

