################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

BINPLACE = bin

OBJS = control.o  \
       crypto.o   \
       data.o     \
       eapol.o    \
       mgmt.o     \
       net80211.o \
       netlink.o  \

DYNLIBS = $(BINROOT)/kernel      \
          $(BINROOT)/netcore.drv \

include $(SRCROOT)/os/minoca.mk

