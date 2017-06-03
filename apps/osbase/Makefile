################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Minoca OS Library
#
#   Abstract:
#
#       This module implements the native system call interface between user
#       mode applications and the kernel. It runs in user mode, and is utilized
#       by the C library (or by applications directly) as an interface to the
#       kernel. Applications are permitted to link against this library and
#       call functions exported by it, but are not allowed to make system calls
#       themselves (most of this library is simply a function call veneer over
#       the system calls anyway). Applications utilizing this native library
#       can get added functionality or performance, but at the cost of
#       portability.
#
#   Author:
#
#       Evan Green 25-Feb-2013
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = libminocaos.so.1

BINARYTYPE = so

BINPLACE = bin

ENTRY = OsDynamicLoaderMain

OBJS = env.o       \
       heap.o      \
       osimag.o    \
       osbase.o    \
       rwlock.o    \
       socket.o    \
       spinlock.o  \
       time.o      \
       tls.o       \

ARMV7_OBJS = armv7/features.o \
             armv7/osbasea.o  \
             armv7/syscall.o  \

ARMV6_OBJS = $(ARMV7_OBJS)

X86_OBJS = x86/features.o \
           x86/osbasea.o \
           x86/syscall.o \

X64_OBJS = x64/osbasea.o \
           x64/syscall.o \

EXTRA_LDFLAGS += -Wl,-Bsymbolic
EXTRA_LDFLAGS += -nostdlib -Wl,--whole-archive

EXTRA_SRC_DIRS = x86 x64 armv7

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/basertl.a        \
             $(OBJROOT)/os/lib/rtl/base/wide/basertlw.a  \
             $(OBJROOT)/os/lib/rtl/urtl/urtl.a           \
             $(OBJROOT)/os/lib/im/native/imn.a           \
             $(OBJROOT)/os/lib/crypto/crypto.a           \

include $(SRCROOT)/os/minoca.mk

$(OBJROOT)/os/apps/osbase/urtl/urtl.a: urtl

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@cp -p $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib

