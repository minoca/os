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
#       Debug Test
#
#   Abstract:
#
#       This executable implements the debug API test application.
#
#   Author:
#
#       Evan Green 15-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = dbgtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = dbgtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

