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

OS ?= $(shell uname -s)

ifeq ($(OS),$(filter Windows_NT cygwin,$(OS)))

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

endif

DIRS = dynamic \

EXTRA_ASFLAGS += -Wa,-I$(SRCDIR)/..

include $(SRCROOT)/os/minoca.mk

dynamic: $(BINARY)
gram.c:
	@echo Generating Grammar - $@
	@../gram/gramgen $@

ckcore.o: ckcore.ck

