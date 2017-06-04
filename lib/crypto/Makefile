################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = klibrary

include $(SRCDIR)/sources

DIRS = build    \
       ssl

TESTDIRS = testcryp

include $(SRCROOT)/os/minoca.mk

testcryp: build ssl

