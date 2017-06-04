################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       BCM2709 UEFI Device Library
#
#   Abstract:
#
#       This library contains support for the BCM2709 SoC's devices.
#
#   Author:
#
#       Chris Stevens 18-Mar-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

include $(SRCDIR)/../common

BINARY = bcm2709.a

BINARYTYPE = klibrary

OBJS = gpio.o     \
       init.o     \
       intr.o     \
       mailbox.o  \
       memmap.o   \
       pwm.o      \
       sd.o       \
       serial.o   \
       timer.o    \
       usb.o      \
       video.o    \

include $(SRCROOT)/os/minoca.mk

