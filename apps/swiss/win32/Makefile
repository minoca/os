################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINARY = swiss.exe

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = ntconsole

INCLUDES += $(SRCROOT)/os/apps/libc/dynamic/wincsup/include;$(SRCDIR)/..;.;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

X86_OBJS = $(COMMON_OBJS)   \
           dw.o             \
           swiss.rsc        \
           w32cmds.o        \
           sh/shntos.o      \
           swlib/ntos.o     \

TARGETLIBS = $(OBJROOT)/os/apps/libc/dynamic/wincsup/wincsup.a               \
             $(OBJROOT)/os/lib/termlib/build/termlib.a                       \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a                         \
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

