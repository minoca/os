################################################################################
#
#   Copyright (c) 2016 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Chalk
#
#   Abstract:
#
#       This executable implements the Chalk interactive interpreter.
#
#   Author:
#
#       Evan Green 26-May-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = chalk

BINARYTYPE = app

BINPLACE = bin

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/dynamic/libchalk.so.1

TARGETLIBS = $(OBJROOT)/os/apps/ck/modules/app/app.a       \
             $(OBJROOT)/os/apps/ck/modules/bundle/bundle.a \

EXTRA_LDFLAGS += -Wl,-rpath='$$ORIGIN/../lib'

include $(SRCDIR)/sources

OBJS += $(POSIX_OBJS)

DIRS = build

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/apps/usr/bin
	@$(STRIP) -p -o $(BINROOT)/apps/usr/bin/$(BINARY) $(BINROOT)/$(BINARY)

