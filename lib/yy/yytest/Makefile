################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       YyTest
#
#   Abstract:
#
#       This program compiles the Lexer/Parser Library into an application and
#       tests it.
#
#   Author:
#
#       Evan Green 9-Oct-2015
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = yytest

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/build/basertl.a          \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a              \
             $(INTLIB)/os/lib/yy/build/yy.a                     \

OBJS = yytest.o \

include $(SRCROOT)/os/minoca.mk

