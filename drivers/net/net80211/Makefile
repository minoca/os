################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Net 802.11 Core
#
#   Abstract:
#
#       This module implements the IEEE 802.11 networking core. It manages 
#       802.11 wireless network traffic.
#
#   Author:
#
#       Chris Stevens 22-Oct-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = net80211.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = control.o  \
       data.o     \
       mgmt.o     \
       net80211.o \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/netcore.drv        \

include $(SRCROOT)/os/minoca.mk

