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
#       USB Core
#
#   Abstract:
#
#       This module implements the USB core. It manages host controllers,
#       enumerates devices, manages transfers, and generally provides the high
#       level glue needed to make USB work.
#
#   Author:
#
#       Evan Green 15-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbcore.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = enum.o     \
       hub.o      \
       usbcore.o  \
       usbhost.o  \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

