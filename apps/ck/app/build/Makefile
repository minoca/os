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
#       Chalk (Build)
#
#   Abstract:
#
#       This executable implements the Chalk interactive interpreter.
#
#   Author:
#
#       Evan Green 28-Aug-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = chalk

BINARYTYPE = build

BUILD = yes

BINPLACE = tools/bin

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

TARGETLIBS = $(OBJROOT)/os/apps/ck/modules/app/build/app.a       \
             $(OBJROOT)/os/apps/ck/modules/bundle/build/bundle.a \

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

OBJS += $(WIN32_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/chalk.dll

else

OBJS += $(POSIX_OBJS)

ifeq ($(OS),Darwin)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/libchalk.1.dylib
EXTRA_LDFLAGS += -Wl,-rpath,@executable_path/../lib

else

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/libchalk.so.1
EXTRA_LDFLAGS += -Wl,-rpath='$$ORIGIN/../lib'
endif
endif

include $(SRCROOT)/os/minoca.mk

