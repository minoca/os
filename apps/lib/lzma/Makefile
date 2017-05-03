################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       LZMA Library
#
#   Abstract:
#
#       This module contains support for compressing and decompressing using
#       the lzma algorithm.
#
#   Author:
#
#       Evan Green 27-Oct-2016
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = liblzma.a

BINARYTYPE = library

BINPLACE = bin

include $(SRCDIR)/sources

DIRS = build \
       util  \

include $(SRCROOT)/os/minoca.mk

util: $(BINARY) build

