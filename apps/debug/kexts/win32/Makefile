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

BINARYTYPE = so

BINPLACE = tools/bin

VPATH += $(SRCDIR)/..:

BUILD = yes

TARGETLIBS = $(OBJROOT)/os/apps/debug/dbgext/win32/dbgextnt.a

OBJS = acpiext.o  \
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

