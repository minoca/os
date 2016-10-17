################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS (Build Shared)
#
#   Abstract:
#
#       This shared Chalk module implements core OS level functionality.
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

BINARY := os.so

BINARYTYPE = so

BUILD = yes

BINPLACE = tools/lib/chalk1

VPATH += ..:

include $(SRCDIR)/../../sources

ifeq ($(OS),Windows_NT)

BINARY := os.dll
BINARYTYPE := dll

# OBJS += $(WIN32_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/chalk.dll

else

# OBJS += $(POSIX_OBJS)

endif

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Darwin)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/dynamic/libchalk.1.dylib

endif

