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
#       Base Video Library
#
#   Abstract:
#
#       This module implements basic support for video output via a linear
#       framebuffer.
#
#   Author:
#
#       Evan Green 30-Jan-2013
#
#   Environment:
#
#       Any
#
################################################################################

BINARY = basevid.a

BINARYTYPE = klibrary

OBJS = fontdata.o \
       textvid.o  \

ifeq ($(ARCH),x64)
DIRS += x6432
endif

include $(SRCROOT)/os/minoca.mk

