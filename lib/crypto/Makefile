################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Crypto Library
#
#   Abstract:
#
#       This library contains the Cryptographic Library functions used
#       throughout Minoca OS.
#
#   Author:
#
#       Evan Green 13-Jan-2015
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = crypto.a

BINARYTYPE = library

include $(SRCDIR)/sources

DIRS = build    \
       ssl

TESTDIRS = testcryp

include $(SRCROOT)/os/minoca.mk

testcryp: build ssl

