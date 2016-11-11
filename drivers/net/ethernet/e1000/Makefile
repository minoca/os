################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Intel e1000
#
#   Abstract:
#
#       This module implements the Intel E1000 Ethernet controller driver.
#
#   Author:
#
#       Evan Green 8-Nov-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = e1000.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = e1000.o    \
       e1000hw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

