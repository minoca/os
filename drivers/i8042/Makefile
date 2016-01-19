################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       i8042 (Keyboard)
#
#   Abstract:
#
#       This module implements a keyboard and mouse driver for the Intel 8042
#       keyboard controller.
#
#   Author:
#
#       Evan Green 20-Dec-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = i8042.drv

BINARYTYPE = so

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/drivers/usrinput/inc;

OBJS = i8042.o    \
       scancode.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usrinput.drv           \

DIRS = pl050

include $(SRCROOT)/os/minoca.mk

