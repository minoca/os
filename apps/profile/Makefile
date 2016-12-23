################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       profile
#
#   Abstract:
#
#       This executable implements the profile application. It is used to
#       enable and disable the Minoca System Profiler.
#
#   Author:
#
#       Chris Stevens 18-Jan-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = profile

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = profile.o \

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@$(STRIP) -p -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY)

