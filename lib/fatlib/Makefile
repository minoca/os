################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       FAT Library
#
#   Abstract:
#
#       This module implements support for the FAT file system.
#
#   Author:
#
#       Evan Green 23-Sep-2012
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = fat.a

BINARYTYPE = klibrary

include $(SRCDIR)/sources

DIRS = build

TESTDIRS = fattest

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

fattest: build

