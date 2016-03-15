################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
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

INCLUDES += $(SRCROOT)/os/apps/include;      \
            $(SRCROOT)/os/apps/include/libc; \

OBJS = netcon.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/netcon; then \
	    $(STRIP) -o $(BINROOT)/skel/bin/netcon $(BINROOT)/$(BINARY); \
	fi
