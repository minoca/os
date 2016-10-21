################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
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

include $(SRCDIR)/sources

OBJS += $(POSIX_OBJS)

DIRS = build

include $(SRCROOT)/os/minoca.mk

