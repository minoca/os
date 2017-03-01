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
#       RK32xx GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the RockChip
#       RK32xx SoC.
#
#   Author:
#
#       Evan Green 25-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rk32gpio.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = rk32gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk

