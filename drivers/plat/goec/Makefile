################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

BINPLACE = bin

OBJS = goec.o   \
       keymap.o \

DYNLIBS = $(BINROOT)/kernel              \
          $(BINROOT)/usrinput.drv        \

include $(SRCROOT)/os/minoca.mk

