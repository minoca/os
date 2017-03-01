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
#       Device Removal Test
#
#   Abstract:
#
#       This module implements a test driver that handles device removal.
#
#   Author:
#
#       Chris Stevens 13-May-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = devrem.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = devrem.o    \

DYNLIBS = $(BINROOT)/kernel             \

include $(SRCROOT)/os/minoca.mk

