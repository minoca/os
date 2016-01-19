################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

INCLUDES += $(SRCROOT)/os/apps/inc;$(SRCROOT)/os/apps/inc/libc;

OBJS = init.o   \
       atexit.o \

ARMV7_OBJS = armv7/aatexit.o \
             armv7/crt0.o    \

ARMV6_OBJS = armv7/aatexit.o \
             armv7/crt0.o    \

X86_OBJS = x86/crt0.o   \

X64_OBJS = x64/crt0.o   \

EXTRA_SRC_DIRS = x86 x64 armv7

include $(SRCROOT)/os/minoca.mk

