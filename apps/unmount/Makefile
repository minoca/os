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

INCLUDES += $(SRCROOT)/os/apps/inc;      \
            $(SRCROOT)/os/apps/inc/libc; \

OBJS = unmount.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@if test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/bin/umount; then \
	    $(STRIP) -o $(BINROOT)/skel/bin/umount $(BINROOT)/$(BINARY); \
	fi

