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
#       USB Mass Storage
#
#   Abstract:
#
#       This module implements the USB Mass Storage class interface driver.
#
#   Author:
#
#       Evan Green 28-Jan-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = usbmass.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = usbmass.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

