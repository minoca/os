################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Minoca Debugger
#
#   Abstract:
#
#       This program implements the debugger (command line version), targeted
#       at Minoca OS.
#
#   Author:
#
#       Evan Green 27-May-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = debug

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCDIR)/..;                        \
            $(SRCROOT)/os/lib/im;                \
            $(SRCROOT)/os/apps/libc/include;

TARGETLIBS = $(OBJROOT)/os/lib/im/imu.a          \

DYNLIBS = -lminocaos

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       ptrace.o         \
       uos/commos.o     \
       uos/extsup.o     \
       uos/sock.o       \

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@$(STRIP) -p -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY)

