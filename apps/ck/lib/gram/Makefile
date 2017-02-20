################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

TARGETLIBS = $(OBJROOT)/os/lib/yy/gen/yygen.a                    \

OBJS = gramgen.o \

include $(SRCROOT)/os/minoca.mk

