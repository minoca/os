################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Chalk (Shared)
#
#   Abstract:
#
#       This shared library contains the Chalk interpreter, which provides a
#       basic programming language.
#
#   Author:
#
#       Evan Green 9-Jun-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = libchalk.so.1

BINARYTYPE = so

BINPLACE = bin

VPATH += ..:

include $(SRCDIR)/../sources

OBJS += $(POSIX_OBJS)

TARGETLIBS = $(OBJROOT)/os/lib/yy/yy.a                     \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/apps/usr/lib
	@$(STRIP) -p -o $(BINROOT)/apps/usr/lib/$(BINARY) $(BINROOT)/$(BINARY)

