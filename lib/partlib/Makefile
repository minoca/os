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
#       Partition Library
#
#   Abstract:
#
#       This module implements support for disk partitions.
#
#   Author:
#
#       Evan Green 30-Jan-2014
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = partlib.a

BINARYTYPE = klibrary

include $(SRCDIR)/sources

DIRS = build

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

