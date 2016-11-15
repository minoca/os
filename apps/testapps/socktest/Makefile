################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Socket Test
#
#   Abstract:
#
#       This executable implements the socket test application.
#
#   Author:
#
#       Evan Green 6-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = socktest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = socktest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

