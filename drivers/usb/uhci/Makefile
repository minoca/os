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
#       UHCI
#
#   Abstract:
#
#       This module implements the UHCI USB 1.1 Host Controller Driver.
#
#   Author:
#
#       Evan Green 13-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = uhci.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = uhci.o     \
       uhcihc.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

