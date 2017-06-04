################################################################################
#
#   Copyright (c) 2017 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       FAT Library (32-bit)
#
#   Abstract:
#
#       This module implements support for the FAT file system.
#
#   Author:
#
#       Evan Green 2-Jun-2017
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = fat.a

BINARYTYPE = klibrary

include $(SRCDIR)/../sources

VPATH += $(SRCDIR)/..:

EXTRA_CPPFLAGS += -m32

include $(SRCROOT)/os/minoca.mk

