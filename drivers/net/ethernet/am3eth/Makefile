################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       AM335x CPSW Ethernet Controller
#
#   Abstract:
#
#       This module implements the TI CPSW Ethernet Controller driver found on
#       TI AM335x devices.
#
#   Author:
#
#       Evan Green 20-Mar-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3eth.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = am3eth.o    \
       am3ethhw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

