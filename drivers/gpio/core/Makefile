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
#       GPIO Core
#
#   Abstract:
#
#       This module implements the GPIO core support library. It provides
#       generic support infrastructure for all GPIO drivers.
#
#   Author:
#
#       Evan Green 4-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = gpio.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = gpio.o     \

DYNLIBS = $(BINROOT)/kernel \

include $(SRCROOT)/os/minoca.mk

