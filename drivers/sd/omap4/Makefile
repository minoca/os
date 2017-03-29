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
#       SD OMAP4
#
#   Abstract:
#
#       This module implements the OMAP4 SD controller driver.
#
#   Author:
#
#       Evan Green 16-Mar-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sdomap4.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = i2c.o        \
       pmic.o       \
       sdomap4.o    \

DYNLIBS = $(BINROOT)/kernel             \
          $(BINROOT)/sd.drv             \

include $(SRCROOT)/os/minoca.mk

