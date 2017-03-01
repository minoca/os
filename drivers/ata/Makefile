################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       ATA
#
#   Abstract:
#
#       This module implements the driver for the AT Attachment (ATA)
#       controller.
#
#   Author:
#
#       Evan Green 5-Jun-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ata.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = ata.o

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

