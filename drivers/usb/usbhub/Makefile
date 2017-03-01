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
#       USB Hub
#
#   Abstract:
#
#       This module implements the USB Hub class driver.
#
#   Author:
#
#       Evan Green 16-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbhub.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = usbhub.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

