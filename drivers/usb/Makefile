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

USB_HOST_CONTROLLERS = am3usb  \
                       dwhci   \
                       ehci    \
                       uhci    \

USB_CLASS_DRIVERS = usbcomp    \
                    usbhub     \
                    usbmass    \

USB_DEVICE_DRIVERS = onering   \
                     usbkbd    \

DIRS = usbcore                 \
       $(USB_CLASS_DRIVERS)    \
       $(USB_DEVICE_DRIVERS)   \
       $(USB_HOST_CONTROLLERS) \

include $(SRCROOT)/os/minoca.mk

$(USB_HOST_CONTROLLERS) $(USB_CLASS_DRIVERS) $(USB_DEVICE_DRIVERS): usbcore

