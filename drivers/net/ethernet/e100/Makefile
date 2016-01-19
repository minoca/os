################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Intel e100
#
#   Abstract:
#
#       This module implements the Intel E100 integrated LAN controller
#       driver (sometimes known as eepro100).
#
#   Author:
#
#       Evan Green 4-Apr-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = e100.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = e100.o    \
       e100hw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

