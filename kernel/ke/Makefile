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
#       Kernel Executive
#
#   Abstract:
#
#       This library contains the Kernel Executive, which is provides high
#       level kernel services such as timekeeping, work queues, DPCs,
#       scheduling, and synchronization primitives.
#
#   Author:
#
#       Evan Green 5-Aug-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ke.a

BINARYTYPE = klibrary

##
## Define the kernel version.
##

VERSION_MAJOR := 0
VERSION_MINOR := 4
VERSION_REVISION := 0
VERSION_RELEASE := SystemReleaseDevelopment

OBJS = crash.o    \
       crashdmp.o \
       dpc.o      \
       event.o    \
       info.o     \
       init.o     \
       ipi.o      \
       lock.o     \
       random.o   \
       reset.o    \
       runlevel.o \
       sched.o    \
       syscall.o  \
       sysclock.o \
       sysres.o   \
       timer.o    \
       version.o  \
       video.o    \
       workitem.o \

X86_OBJS = x86/archinit.o \
           x86/ctxswap.o  \
           x86/ctxswapc.o \
           x86/dispatch.o \
           x86/pcexcept.o \
           x86/proc.o     \

X64_OBJS = x64/ctxswap.o  \
           x64/ctxswapc.o \
           x64/dispatch.o \
           x86/archinit.o \
           x86/pcexcept.o \
           x86/proc.o     \

ARMV7_OBJS = armv7/archinit.o \
             armv7/ctxswap.o  \
             armv7/ctxswapc.o \
             armv7/dispatch.o \
             armv7/proc.o     \

ARMV6_OBJS = armv7/archinit.o \
             armv7/ctxswap.o  \
             armv7/ctxswapc.o \
             armv7/dispatch.o \
             armv7/proc.o     \

EXTRA_SRC_DIRS = x86 x64 armv7 armv6

INCLUDES += .;

include $(SRCROOT)/os/minoca.mk

##
## Create the version header.
##

version.o: $(CURDIR)/version.h

$(CURDIR)/version.h: $(BINROOT)/kernel-version
	$(GEN_VERSION) $@ header $(VERSION_MAJOR) $(VERSION_MINOR) \
	    $(VERSION_REVISION) $(VERSION_RELEASE)

$(BINROOT)/kernel-version:
	$(GEN_VERSION) $@ simple $(VERSION_MAJOR) $(VERSION_MINOR) \
	    $(VERSION_REVISION) $(VERSION_RELEASE)
