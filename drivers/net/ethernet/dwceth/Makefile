################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       DesignWare Ethernet
#
#   Abstract:
#
#       This module implements the Synopsys DesignWare Ethernet network driver.
#
#   Author:
#
#       Evan Green 5-Dec-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = dwceth.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = dwceth.o    \
       dwcethhw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

