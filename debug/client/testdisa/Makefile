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

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a   \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a      \
             $(OBJROOT)/os/lib/im/build/im.a              \

VPATH += $(SRCDIR)/..:

OBJS = testdisa.o   \
       x86dis.o     \
       armdis.o     \
       disasm.o     \
       thmdis.o     \
       thm32dis.o   \

X86_OBJS = testdata.o \

include $(SRCROOT)/os/minoca.mk

