################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       DWHCI
#
#   Abstract:
#
#       This module implements a DesignWare High-Speed USB 2.0 On-The-Go
#       (HS OTG) Host Controller Driver.
#
#   Author:
#
#       Chris Stevens 27-Mar-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = dwhci.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = dwhci.o     \
       dwhcihc.o   \

INCLUDES += ..;$(SRCROOT)/os/drivers/usb/usbcore/inc;

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

