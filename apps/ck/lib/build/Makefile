################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk Library (Build)
#
#   Abstract:
#
#       This library contains the Chalk interpreter targeted toward the build
#       machine environment.
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

BINARY := libchalk.so.1

BINARYTYPE = so

BUILD = yes

BINPLACE = tools/bin

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

ENTRY := 0

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

BINARY := chalk.dll
BINARYTYPE := dll

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

endif

TARGETLIBS = $(OBJROOT)/os/lib/yy/build/yy.a                     \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a             \

include $(SRCROOT)/os/minoca.mk

ifneq ($(OS),$(filter Windows_NT Minoca,$(OS)))

DYNLIBS += -ldl

endif

gram.c:
	@echo Generating Grammar - $@
	@../gram/gramgen $@

ckcore.o: ckcore.ck
