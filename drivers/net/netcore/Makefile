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
       buf.o             \
       ethernet.o        \
       mcast.o           \
       netcore.o         \
       raw.o             \
       tcp.o             \
       tcpcong.o         \
       udp.o             \
       ipv4/arp.o        \
       ipv4/dhcp.o       \
       ipv4/igmp.o       \
       ipv4/ip4.o        \
       ipv6/icmp6.o      \
       ipv6/ip6.o        \
       ipv6/mld.o        \
       ipv6/ndp.o        \
       netlink/netlink.o \
       netlink/genctrl.o \
       netlink/generic.o \

EXTRA_SRC_DIRS = ipv4    \
                 ipv6    \
                 netlink

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

