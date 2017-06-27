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
#       Chalk Library (Build, Shared)
#
#   Abstract:
#
#       This shared library contains the Chalk interpreter targeted toward the
#       build machine environment.
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

SO_VERSION_MAJOR := 1
SO_VERSION_MINOR := 0

BINARY := libchalk.so.$(SO_VERSION_MAJOR)

BINARYTYPE = so

BUILD = yes

BINPLACE = tools/lib

INCLUDES += $(SRCDIR)/..;

VPATH += ..:

include $(SRCDIR)/../../sources

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

BINARY := chalk.dll
BINPLACE := tools/bin

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

ifeq ($(shell uname -s),Darwin)
BINARY := libchalk.$(SO_VERSION_MAJOR).dylib
EXTRA_LDFLAGS += -install_name @rpath/libchalk.$(SO_VERSION_MAJOR).dylib
endif

endif

TARGETLIBS = $(OBJROOT)/os/lib/yy/build/yy.a                     \

include $(SRCROOT)/os/minoca.mk

ifneq ($(OS),$(filter Windows_NT Minoca FreeBSD,$(OS)))

DYNLIBS += -ldl

endif

