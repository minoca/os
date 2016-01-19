################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Kernel Debugger Extensions (Win32)
#
#   Abstract:
#
#       This module builds the kernel debugger extensions for a Windows
#       debugging host.
#
#   Author:
#
#       Evan Green 10-Sep-2012
#
#   Environment:
#
#       Win32
#
################################################################################

BINARY = kexts.dll

BINARYTYPE = dll

BINPLACE = bin

INCLUDES += $(SRCDIR)/../..;

VPATH += $(SRCDIR)/..:

BUILD = yes

TARGETLIBS = $(BINROOT)/dbgextnt.a

X86_OBJS = acpiext.o  \
           kexts.o    \
           memory.o   \
           objects.o  \
           reslist.o  \
           threads.o  \

include $(SRCROOT)/os/minoca.mk

##
## Override the compiler since this is a Windows library.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as

