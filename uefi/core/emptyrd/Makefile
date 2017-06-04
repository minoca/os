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
#       Empty RAM Disk
#
#   Abstract:
#
#       This file contains the symbols for a RAM disk image, but with nothing
#       in them.
#
#   Author:
#
#       Evan Green 5-Oct-2016
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = emptyrd.a

BINARYTYPE = klibrary

INCLUDES += $(SRCROOT)/os/uefi/include;.;

OBJS = emptyrd.o  \

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

