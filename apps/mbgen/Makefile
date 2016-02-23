################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mbgen
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

BINARY = mbgen

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/include;             \
            $(SRCROOT)/os/apps/include/libc;        \
            $(SRCROOT)/os/apps/lib/chalk;       \

include $(SRCDIR)/sources

TARGETLIBS = $(OBJROOT)/os/apps/lib/chalk/chalk.a           \
             $(OBJROOT)/os/lib/yy/yy.a                      \
             $(OBJROOT)/os/lib/rtl/rtlc/rtlc.a              \
             $(OBJROOT)/os/lib/rtl/base/basertl.a           \

DIRS = build

include $(SRCROOT)/os/minoca.mk

