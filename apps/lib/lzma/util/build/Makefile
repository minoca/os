################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       lzma (Build)
#
#   Abstract:
#
#       This executable implements the lzma utility for the build environment.
#
#   Author:
#
#       Evan Green 13-Jan-2016
#
#   Environment:
#
#       User
#
################################################################################

BINARY = lzma

BINPLACE = tools/bin

BINARYTYPE = build

BUILD = yes

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

TARGETLIBS = $(OBJROOT)/os/apps/lib/lzma/build/liblzma.a            \

include $(SRCROOT)/os/minoca.mk

