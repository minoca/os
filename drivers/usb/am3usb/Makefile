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
#       TI AM33xx USB
#
#   Abstract:
#
#       This module implements the USB 2.0 OTG controller on TI AM33xx SoCs.
#
#   Author:
#
#       Evan Green 11-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3usb.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = am3usb.o       \
       am3usbhw.o     \
       cppi.o         \
       musb.o         \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

