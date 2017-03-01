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
#       AM335x SoC
#
#   Abstract:
#
#       This module implements the TI AM335x SoC support driver.
#
#   Author:
#
#       Evan Green 9-Sep-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = am3soc.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = am3soc.o   \
       am3cm3fw.o \
       mailbox.o  \
       sleep.o    \

FW_BIN = $(FW).bin

DYNLIBS = $(BINROOT)/kernel              \

include $(SRCROOT)/os/minoca.mk

am3cm3fw.o: am3cm3fw.bin

