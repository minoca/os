################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Net Core
#
#   Abstract:
#
#       This module implements the networking core. It manages network
#       interfaces and provides support for core protocols like TCP, IP, IPv6,
#       ARP, and others.
#
#   Author:
#
#       Evan Green 4-Apr-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = netcore.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = addr.o            \
       arp.o             \
       buf.o             \
       dhcp.o            \
       ethernet.o        \
       igmp.o            \
       ip4.o             \
       netcore.o         \
       raw.o             \
       tcp.o             \
       tcpcong.o         \
       udp.o             \
       netlink/netlink.o \
       netlink/genctrl.o \
       netlink/generic.o \

EXTRA_SRC_DIRS = netlink

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

