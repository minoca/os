################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       umount
#
#   Abstract:
#
#       This executable implements the unmount application.
#
#   Author:
#
#       Chris Stevens 30-Jul-2013
#
#   Environment:
#
#       User
#
################################################################################

BINARY = umount

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/include;      \
            $(SRCROOT)/os/apps/include/libc; \

OBJS = unmount.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/$(BINARY); then \
	    $(STRIP) -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY); \
	fi

