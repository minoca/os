################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Chalk
#
#   Abstract:
#
#       This executable implements the Chalk interactive interpreter.
#
#   Author:
#
#       Evan Green 26-May-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = chalk

BINARYTYPE = build

BUILD = yes

BINARYTYPE = build

BINPLACE = bin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a             \
             $(OBJROOT)/os/lib/yy/build/yy.a                     \
             $(OBJROOT)/os/apps/ck/lib/build/chalk.a             \

OBJS = chalk.o \

ifeq ($(OS),Windows_NT)

OBJS += ckwin32.o \

else

OBJS += ckunix.o \

endif

include $(SRCROOT)/os/minoca.mk

