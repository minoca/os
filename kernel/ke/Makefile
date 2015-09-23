################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
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

BINARYTYPE = library

BINPLACE = intlib

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
       timezone.o \
       tpc.o      \
       version.o  \
       workitem.o \

X86_OBJS = x86/archinit.o \
           x86/ctxswap.o  \
           x86/ctxswapc.o \
           x86/dispatch.o \
           x86/proc.o     \

ARMV7_OBJS = armv7/archinit.o \
             armv7/ctxswap.o  \
             armv7/ctxswapc.o \
             armv7/dispatch.o \
             armv7/proc.o     \

ARMV6_OBJS = armv6/ctxswap.o  \
             armv7/archinit.o \
             armv7/ctxswapc.o \
             armv7/dispatch.o \
             armv7/proc.o     \

EXTRA_SRC_DIRS = x86 armv7 armv6

include $(SRCROOT)/os/minoca.mk

