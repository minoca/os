################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       OS
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

BINARY = os.so

BINARYTYPE = so

include $(SRCDIR)/sources

# OBJS += $(POSIX_OBJS)

DIRS = build \

DYNLIBS = $(OBJROOT)/os/apps/ck/lib/libchalk.so.1

ENTRY := 0

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/chalk1
	@if test $(BINARY) -nt $(BINROOT)/chalk1/$(BINARY); then \
	    cp $(BINARY) $(BINROOT)/chalk1/$(BINARY); \
	fi

