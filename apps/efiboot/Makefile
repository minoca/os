################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       efiboot
#
#   Abstract:
#
#       This executable implements support for the efiboot program, which is a
#       user mode utility that can be used to change EFI boot options.
#
#   Author:
#
#       Evan Green 9-Dec-2014
#
#   Environment:
#
#       User
#
################################################################################

BINARY = efiboot

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include; \

OBJS = efiboot.o \

CFLAGS += -fshort-wchar

ifeq ($(ARCH), $(filter armv7 armv6,$(ARCH)))
LDFLAGS += -Wl,--no-wchar-size-warning
endif

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

postbuild:
	@mkdir -p $(BINROOT)/skel/bin
	@$(STRIP) -p -o $(BINROOT)/skel/bin/$(BINARY) $(BINROOT)/$(BINARY)

