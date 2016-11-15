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
#       Timer Test
#
#   Abstract:
#
#       This executable implements the user mode timer test application.
#
#   Author:
#
#       Evan Green 11-Aug-2013
#
#   Environment:
#
#       User
#
################################################################################

BINARY = utmrtest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = utmrtest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

