################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

INCLUDES += $(SRCROOT)/os/apps/inc;

OBJS = env.o       \
       heap.o      \
       osimag.o    \
       osbase.o    \
       socket.o    \
       spinlock.o  \
       time.o      \
       tls.o       \

ARMV7_OBJS = armv7/osbasea.o \
             armv7/syscall.o \

ARMV6_OBJS = armv7/osbasea.o \
             armv7/syscall.o \

X86_OBJS = x86/osbasea.o \
           x86/syscall.o \

X64_OBJS = x64/osbasea.o \
           x64/syscall.o \

##
## ARM binutils starts binaries at 0x8000, so leave enough room for a large
## static executable.
##

ifeq ($(ARCH), $(filter armv7 armv6,$(ARCH)))
EXTRA_LDFLAGS += -Wl,-Ttext-segment=0x10000000
else
EXTRA_LDFLAGS += -Wl,-Ttext-segment=0x200000
endif

EXTRA_LDFLAGS += -Wl,-Bsymbolic
EXTRA_LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib -Wl,-whole-archive

EXTRA_SRC_DIRS = x86 x64 armv7

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/basertl.a        \
             $(INTLIB)/os/lib/rtl/base/wide/basertlw.a  \
             $(INTLIB)/os/lib/im/im.a                   \
             $(INTLIB)/os/apps/osbase/urtl/urtl.a       \
             $(INTLIB)/os/lib/crypto/crypto.a           \

DIRS = urtl    \

include $(SRCROOT)/os/minoca.mk

$(INTLIB)/os/apps/osbase/urtl/urtl.a: urtl

postbuild: $(BINROOT)/kernel-version

postbuild:
	@mkdir -p $(BINROOT)/skel/lib
	@if test -d $(BINROOT)/skel/lib && \
	    test $(BINROOT)/$(BINARY) -nt $(BINROOT)/skel/lib/$(BINARY); then \
	    cp $(BINROOT)/$(BINARY) $(BINROOT)/skel/lib/ ; \
	fi

$(BINROOT)/kernel-version: $(BINARY)
	@printf $(SYSTEM_VERSION_MAJOR).$(SYSTEM_VERSION_MINOR). > $@ ; \
	if [ -r $(SRCROOT)/os/revision ]; then \
	    cat $(SRCROOT)/os/revision >> $@ ; \
	else \
	    printf "$(REVISION)" >> $@ ; \
	fi

