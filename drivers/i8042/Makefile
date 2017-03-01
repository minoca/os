################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

BINPLACE = bin

OBJS = i8042.o    \
       scancode.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usrinput.drv           \

DIRS = pl050

include $(SRCROOT)/os/minoca.mk

