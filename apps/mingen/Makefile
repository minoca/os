################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mingen
#
#   Abstract:
#
#       This executable implements the Minoca build generator.
#
#   Author:
#
#       Evan Green 3-Dec-2015
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = mingen

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;        \
            $(SRCROOT)/os/apps/lib/chalk;       \

include $(SRCDIR)/sources

OBJS += uos.o

TARGETLIBS = $(OBJROOT)/os/apps/lib/chalk/chalk.a           \
             $(OBJROOT)/os/lib/yy/yy.a                      \
             $(OBJROOT)/os/lib/rtl/rtlc/rtlc.a              \
             $(OBJROOT)/os/lib/rtl/base/basertl.a           \

DIRS = build

include $(SRCROOT)/os/minoca.mk

