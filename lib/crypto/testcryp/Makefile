################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       TestCryp
#
#   Abstract:
#
#       This program compiles the Cryptographic Library into an application and
#       tests it.
#
#   Author:
#
#       Evan Green 14-Jan-2015
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = testcryp

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/build/basertl.a     \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a         \
             $(INTLIB)/os/lib/crypto/build/crypto.a        \

OBJS = testcryp.o \

include $(SRCROOT)/os/minoca.mk

