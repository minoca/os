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
#       USB Keyboard
#
#   Abstract:
#
#       This module implements the USB Keyboard class interface driver.
#
#   Author:
#
#       Evan Green 20-Mar-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = usbkbd.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = keycode.o  \
       usbkbd.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \
          $(BINROOT)/usrinput.drv           \

include $(SRCROOT)/os/minoca.mk

