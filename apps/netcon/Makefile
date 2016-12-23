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
#       netcon
#
#   Abstract:
#
#       This executable implements the network configuration application.
#
#   Author:
#
#       Chris Stevens 14-Mar-2016
#
#   Environment:
#
#       User
#
################################################################################

BINARY = netcon

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = netcon.o \

DYNLIBS = -lminocaos -lnetlink

LDFLAGS += -L$(BINROOT)

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@$(STRIP) -p -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY)

