################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Mmap Test
#
#   Abstract:
#
#       This executable implements the memory map test application.
#
#   Author:
#
#       Chris Stevens 10-Mar-2014
#
#   Environment:
#
#       User
#
################################################################################

BINARY = mmaptest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = mmaptest.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

