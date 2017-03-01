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
#       Partition
#
#   Abstract:
#
#       This module implements the partition support driver.
#
#   Author:
#
#       Evan Green 30-Jan-2014
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = part.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = part.o  \

DYNLIBS = $(BINROOT)/kernel             \

TARGETLIBS = $(OBJROOT)/os/lib/partlib/partlib.a    \

include $(SRCROOT)/os/minoca.mk

