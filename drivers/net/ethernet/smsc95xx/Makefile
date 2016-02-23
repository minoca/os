################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       SMSC 95xx
#
#   Abstract:
#
#       This module implements support for the SMSC95xx family of USB ethernet
#       controllers.
#
#   Author:
#
#       Evan Green 7-Nov-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = smsc95xx.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = smsc95.o    \
       smsc95hw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

