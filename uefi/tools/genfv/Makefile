################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       GenFV
#
#   Abstract:
#
#       This module builds the GenFV build utility, which can create a FFS2
#       Firmware Volume out of one or more FFS files.
#
#   Author:
#
#       Evan Green 7-Mar-2014
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = genfv

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/include;

OBJS = genfv.o

include $(SRCROOT)/os/minoca.mk

