################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       Google EC
#
#   Abstract:
#
#       This directory contains the driver for the Google Embedded Controller.
#
#   Author:
#
#       Evan Green 25-Aug-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = goec.drv

BINARYTYPE = so

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/drivers/usrinput/inc;

OBJS = goec.o   \
       keymap.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/usrinput.drv        \

include $(SRCROOT)/os/minoca.mk

