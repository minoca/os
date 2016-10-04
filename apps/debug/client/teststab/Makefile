################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a      \
             $(OBJROOT)/os/lib/im/build/im.a              \

VPATH += $(SRCDIR)/..:
INCLUDES += $(SRCROOT)/os/lib/im;

OBJS = teststab.o   \
       stabs.o      \
       coff.o       \
       elf.o        \
       symbols.o    \

include $(SRCROOT)/os/minoca.mk

