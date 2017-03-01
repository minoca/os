################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = generic.o  \
       netlink.o  \

DYNLIBS = $(BINROOT)/libc.so.1           \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@cp -p $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib

