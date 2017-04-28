################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

OBJS = pl050.o    \
       scancode.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/usrinput.drv           \

include $(SRCROOT)/os/minoca.mk

