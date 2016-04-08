################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       mingen (Build)
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

BINARY = mingen

BINPLACE = bin

BUILD = yes

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/apps/include;         \
            $(SRCROOT)/os/apps/lib/chalk;       \

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

OBJS += ntos.o

else

OBJS += uos.o

endif

TARGETLIBS = $(OBJROOT)/os/lib/yy/build/yy.a                   \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a        \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a           \
             $(OBJROOT)/os/apps/lib/chalk/build/chalk.a        \

include $(SRCROOT)/os/minoca.mk

