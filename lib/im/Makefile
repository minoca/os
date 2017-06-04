################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Image Library (Universal)
#
#   Abstract:
#
#       This library contains the Image library used to parse executable
#       binaries in all supported formats.
#
#   Author:
#
#       Evan Green 13-Oct-2012
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = imu.a

BINARYTYPE = klibrary

include $(SRCDIR)/sources

DIRS = build \
       native

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

