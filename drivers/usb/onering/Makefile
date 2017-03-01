################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       One Ring
#
#   Abstract:
#
#       This module implements the USB LED and USB Relay devices from One Ring
#       Road.
#
#   Author:
#
#       Evan Green 15-Jul-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = onering.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = onering.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

DIRS = usbrelay \

include $(SRCROOT)/os/minoca.mk

