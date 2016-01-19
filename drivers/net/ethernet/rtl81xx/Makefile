################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       RTL 81xx
#
#   Abstract:
#
#       This module implements support for the Realtek RTL81xx family of
#       ethernet controllers.
#
#   Author:
#
#       Chris Stevens 20-June-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rtl81xx.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = rtl81.o   \
       rtl81hw.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

