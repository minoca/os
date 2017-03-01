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
#       Serial 16550
#
#   Abstract:
#
#       This module implements support for 16550-like UARTs.
#
#   Author:
#
#       Evan Green 21-Nov-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ser16550.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = ser16550.o    \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

