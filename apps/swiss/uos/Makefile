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
#       swiss (Build)
#
#   Abstract:
#
#       This executable implements the Swiss utility for the build environment,
#       which contains many basic core utilities, including a Bourne-compatible
#       shell.
#
#   Author:
#
#       Evan Green 19-Jan-2016
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = swiss

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = app

INCLUDES += $(SRCDIR)/..;.;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

OBJS = $(COMMON_OBJS)   \
       $(UOS_OBJS)      \
       swlib/linux.o    \
       uoscmds.o        \
       dw.o             \

##
## Add the login related apps on everything except Mac OS and FreeBSD.
##

ifneq ($(shell uname -s),Darwin)
ifneq ($(shell uname -s),FreeBSD)
ifeq ($(findstring CYGWIN,$(shell uname -s)),)
OBJS += $(LOGIN_OBJS)
endif
endif
endif

TARGETLIBS = $(OBJROOT)/os/lib/termlib/build/termlib.a                       \
             $(OBJROOT)/os/lib/rtl/urtl/rtlc/build/rtlc.a                    \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a                      \

ifneq ($(shell uname -s),FreeBSD)
DYNLIBS += -ldl
endif
DYNLIBS += -lutil

include $(SRCROOT)/os/minoca.mk

EXTRA_CFLAGS += -ftls-model=initial-exec

##
## Generate the version header for the commit information.
##

swlib/userio.o: $(CURDIR)/version.h

$(CURDIR)/version.h:
	$(GEN_VERSION) $@ header 0 0 0 SystemReleaseFinal
