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
#       Signal Test
#
#   Abstract:
#
#       This executable implements the signal test application.
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

BINARY = sigtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = sigtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

