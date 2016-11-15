################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = unmount.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@$(STRIP) -p -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY)

