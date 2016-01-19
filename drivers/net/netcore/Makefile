################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINARYTYPE = so

BINPLACE = bin

OBJS = addr.o     \
       arp.o      \
       buf.o      \
       dhcp.o     \
       ethernet.o \
       ip4.o      \
       netcore.o  \
       raw.o      \
       tcp.o      \
       tcpcong.o  \
       udp.o      \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

