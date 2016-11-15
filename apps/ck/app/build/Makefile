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

ifeq ($(OS),Windows_NT)

OBJS += $(WIN32_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/chalk.dll

else

OBJS += $(POSIX_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/libchalk.so.1

endif

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Darwin)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/libchalk.1.dylib

endif
