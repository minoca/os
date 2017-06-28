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
#       swiss (Win32)
#
#   Abstract:
#
#       This executable implements the Swiss utility targeted for Windows
#       platforms.
#
#   Author:
#
#       Evan Green 10-Sep-2013
#
#   Environment:
#
#       Win32
#
################################################################################

BINARY = swiss

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = build

INCLUDES += $(SRCROOT)/os/apps/libc/dynamic/wincsup/include;$(SRCDIR)/..;.;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       dw.o             \
       swiss.rsc        \
       w32cmds.o        \
       sh/shntos.o      \
       swlib/ntos.o     \

TARGETLIBS = $(OBJROOT)/os/apps/libc/dynamic/wincsup/wincsup.a               \
             $(OBJROOT)/os/lib/termlib/build/termlib.a                       \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a                    \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a                      \

DYNLIBS = -lpsapi -lws2_32

include $(SRCROOT)/os/minoca.mk

##
## Generate the version header for the commit information.
##

swlib/userio.o: $(CURDIR)/version.h

$(CURDIR)/version.h:
	$(GEN_VERSION) $@ header 0 0 0 SystemReleaseFinal

##
## Override the compiler since this is a Windows app.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as

