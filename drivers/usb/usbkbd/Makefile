################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINARYTYPE = so

BINPLACE = bin

OBJS = keycode.o  \
       usbkbd.o   \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \
          $(BINROOT)/usrinput.drv           \

include $(SRCROOT)/os/minoca.mk

