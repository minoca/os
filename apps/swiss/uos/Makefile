################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       swiss_build
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

BINARY = swiss_build

BINPLACE = bin

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

TARGETLIBS = $(OBJROOT)/os/lib/termlib/build/termlib.a                       \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a                         \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a                      \

DYNLIBS += -ldl -lutil

include $(SRCROOT)/os/minoca.mk

EXTRA_CFLAGS += -ftls-model=initial-exec

##
## Generate the version header for the commit information.
##

swlib/userio.o: $(CURDIR)/version.h

$(CURDIR)/version.h:
	$(GEN_VERSION) $@ header 0 0 0 SystemReleaseFinal
