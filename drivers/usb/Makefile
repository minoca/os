################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       USB
#
#   Abstract:
#
#       This directory contains Universal Serial Bus (USB) related drivers,
#       including the USB core support library, host controller implementations,
#       generic device class drivers, and specific USB device drivers.
#
#   Author:
#
#       Evan Green 13-Jan-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

DIRS = dwhci     \
       ehci      \
       onering   \
       usbcore   \
       uhci      \
       usbcomp   \
       usbhub    \
       usbkbd    \
       usbmass   \

include $(SRCROOT)/os/minoca.mk

dwhci uhci ehci usbcomp usbhub usbkbd usbmass onering: usbcore

