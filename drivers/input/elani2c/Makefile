################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Elan i2C Touchpad
#
#   Abstract:
#
#       This module implements the Elan i2C touchpad driver.
#
#   Author:
#
#       Evan Green 28-Apr-2017
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = elani2c.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = elani2c.o  \

DYNLIBS = $(BINROOT)/kernel                \
          $(BINROOT)/usrinput.drv

include $(SRCROOT)/os/minoca.mk

