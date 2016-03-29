################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Minoca Netlink Library
#
#   Abstract:
#
#       This module contains the netlink sockets library, which it built on top
#       of the C library socket's interface and offers a set of APIs to make
#       use of netlink sockets easier.
#
#   Author:
#
#       Chris Stevens 24-Mar-2016
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = libnetlink.so.1

BINARYTYPE = so

BINPLACE = bin

ENTRY = NetlinkInitialize

INCLUDES += $(SRCROOT)/os/apps/include;$(SRCROOT)/os/apps/include/libc;

OBJS = generic.o  \
       netlink.o  \

DYNLIBS = $(BINROOT)/libc.so.1           \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/lib/$(BINARY); then \
	    cp -v $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib/ ; \
	fi

