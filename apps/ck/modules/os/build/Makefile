################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS (Build)
#
#   Abstract:
#
#       This Chalk module implements core OS level functionality.
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

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

ENTRY := 0

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

BINARY := os.dll
BINARYTYPE := dll

# OBJS += $(WIN32_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/chalk.dll

else

# OBJS += $(POSIX_OBJS)

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/build/libchalk.so.1

endif

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/build/chalk1
	@if test $(BINARY) -nt $(BINROOT)/build/chalk1/$(BINARY); then \
	    cp $(BINARY) $(BINROOT)/build/chalk1/$(BINARY); \
	fi
