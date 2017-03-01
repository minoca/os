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
#       Simple Peripheral Bus Core
#
#   Abstract:
#
#       This module implements the Simple Peripheral Bus core support library.
#       It provides generic support infrastructure for all SPB drivers, both
#       devices and controllers.
#
#   Author:
#
#       Evan Green 14-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = spb.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = spb.o     \

DYNLIBS = $(BINROOT)/kernel \

include $(SRCROOT)/os/minoca.mk

