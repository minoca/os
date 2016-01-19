################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       ATL1c
#
#   Abstract:
#
#       This module implements a NIC driver for the Atheros ATL1C family
#       (formerly Attansic L1).
#
#   Author:
#
#       Evan Green 18-Apr-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = atl1c.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = atl1c.o   \
       atl1chw.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

