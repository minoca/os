################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       mkuboot
#
#   Abstract:
#
#       This module builds the mkuboot build utility, which can create a U-Boot
#       firmware images.
#
#   Author:
#
#       Chris Stevens 2-Jul-2015
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = mkuboot

BUILD = yes

BINPLACE = tools/bin

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/uefi/include;

VPATH += $(SRCROOT)/os/uefi/core

OBJS = mkuboot.o \
       crc32.o   \

include $(SRCROOT)/os/minoca.mk

