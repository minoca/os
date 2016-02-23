################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       PL-050 Keyboard
#
#   Abstract:
#
#       This module implements a keyboard and mouse driver for the ARM
#       PrimeCell PL050 keyboard and mouse controller.
#
#   Author:
#
#       Evan Green 22-Sep-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = pl050.drv

BINARYTYPE = so

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

OBJS = pl050.o    \
       scancode.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usrinput.drv           \

include $(SRCROOT)/os/minoca.mk

