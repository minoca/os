################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       AMD PCnet32
#
#   Abstract:
#
#       This module implements the Am79C9xx PCnet driver.
#
#   Author:
#
#       Chris Stevens 8-Nov-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = pcnet32.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = pcnet.o    \
       pcnethw.o  \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \

include $(SRCROOT)/os/minoca.mk

