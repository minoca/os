################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
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
             $(OBJROOT)/os/lib/rtl/build/rtlstubs.a       \
             $(OBJROOT)/os/lib/im/build/im.a              \

VPATH += $(SRCDIR)/..:
INCLUDES += $(SRCDIR)/../..;
INCLUDES += $(SRCROOT)/os/lib/im;

OBJS = coff.o        \
       elf.o         \
       dwarf.o       \
       dwexpr.o      \
       dwline.o      \
       dwread.o      \
       stabs.o       \
       symbols.o     \
       tdwarf.o      \

include $(SRCROOT)/os/minoca.mk

