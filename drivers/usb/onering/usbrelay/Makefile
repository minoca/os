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
#       usbrelay
#
#   Abstract:
#
#       This executable implements the usbrelay application, which is a
#       simple application that connects to and communicates with the USB
#       relay board from One Ring Road.
#
#   Author:
#
#       Evan Green 26-Jan-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = usbrelay

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = usbrelay.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

