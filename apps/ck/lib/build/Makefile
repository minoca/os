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

BINARY := libchalk.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

endif

DIRS = dynamic \

include $(SRCROOT)/os/minoca.mk

dynamic: $(BINARY)
gram.c:
	@echo Generating Grammar - $@
	@../gram/gramgen $@

ckcore.o: ckcore.ck

