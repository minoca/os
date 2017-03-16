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
#       USB Mouse
#
#   Abstract:
#
#       This module implements the USB Mouse class interface driver.
#
#   Author:
#
#       Evan Green 14-Mar-2017
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbmouse.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = usbmouse.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \
          $(BINROOT)/usbhid.drv             \
          $(BINROOT)/usrinput.drv           \

include $(SRCROOT)/os/minoca.mk

