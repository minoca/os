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
#       vmstat
#
#   Abstract:
#
#       This executable implements the vmstat application, which prints current
#       information about kernel memory usage.
#
#   Author:
#
#       Evan Green 5-Mar-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = vmstat

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = vmstat.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

