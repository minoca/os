################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       BCM27xx GPIO
#
#   Abstract:
#
#       This module implements General Purpose I/O support for the Broadcom
#       27xx SoCs.
#
#   Author:
#
#       Chris Stevens 10-May-2016
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = bc27gpio.drv

BINARYTYPE = so

BINPLACE = bin

OBJS = bc27gpio.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/gpio.drv            \

include $(SRCROOT)/os/minoca.mk

