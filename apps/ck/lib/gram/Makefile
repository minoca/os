################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       GramGen
#
#   Abstract:
#
#       This program outputs the C source file containing the Chalk grammar
#       parser state tables.
#
#   Author:
#
#       Evan Green 9-May-2016
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = gramgen

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a             \
             $(OBJROOT)/os/lib/yy/gen/yygen.a                    \

OBJS = gramgen.o \

include $(SRCROOT)/os/minoca.mk

