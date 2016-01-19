################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

OBJS = init.o     \
       intr.o     \
       mailbox.o  \
       memmap.o   \
       sd.o       \
       serial.o   \
       timer.o    \
       usb.o      \
       video.o    \

include $(SRCROOT)/os/minoca.mk

