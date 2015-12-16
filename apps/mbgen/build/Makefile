################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mbgen (Build)
#
#   Abstract:
#
#       This executable implements the Minoca Build generator for the build
#       machine.
#
#   Author:
#
#       Evan Green 3-Dec-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = mbgen

BINPLACE = bin

BUILD = yes

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/apps/inc;             \
            $(SRCROOT)/os/apps/lib/chalk;       \

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

TARGETLIBS = $(OBJROOT)/os/lib/yy/build/yy.a                   \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a        \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a           \
             $(OBJROOT)/os/apps/lib/chalk/build/chalk.a        \

include $(SRCROOT)/os/minoca.mk

