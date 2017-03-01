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
#       Static C Library
#
#   Abstract:
#
#       This module contains the portion of the C library that is statically
#       linked into every application. It contains little other than some
#       initialization stubs.
#
#   Author:
#
#       Evan Green 4-Mar-2013
#
#   Environment:
#
#       User Mode C Library
#
################################################################################

BINARY = libc_nonshared.a

BINARYTYPE = library

BINPLACE = bin

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = init.o   \
       atexit.o \
       dlsym.o  \

ARMV7_OBJS = armv7/aatexit.o \
             armv7/crt0.o    \

ARMV6_OBJS = armv7/aatexit.o \
             armv7/crt0.o    \

X86_OBJS = x86/crt0.o   \

X64_OBJS = x64/crt0.o   \

EXTRA_SRC_DIRS = x86 x64 armv7

include $(SRCROOT)/os/minoca.mk

