################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a        \
             $(OBJROOT)/os/lib/crypto/build/crypto.a             \
             $(OBJROOT)/os/lib/crypto/ssl/build/ssl.a            \

OBJS = testcryp.o \

include $(SRCROOT)/os/minoca.mk

