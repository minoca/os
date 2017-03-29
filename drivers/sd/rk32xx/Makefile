################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All rights reserved.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       SD Rockchip 32xx
#
#   Abstract:
#
#       This module implements the Rockchip 32xx SD controller driver.
#
#   Author:
#
#       Chris Stevens 29-Jul-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sdrk32xx.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = sdrk32.o    \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/sd.drv             \

include $(SRCROOT)/os/minoca.mk

