################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Process/Thread
#
#   Abstract:
#
#       This library contains the process and thread library. It maintains the
#       lifecycle of threads (units of execution) and processes (collections of
#       threads in a shared address space).
#
#   Author:
#
#       Evan Green 6-Aug-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = ps.a

BINARYTYPE = library

OBJS = env.o      \
       info.o     \
       init.o     \
       perm.o     \
       pgroups.o  \
       process.o  \
       psimag.o   \
       signals.o  \
       thread.o   \
       usrlock.o  \
       utimer.o   \

ARMV7_OBJS = armv7/psarch.o   \

ARMV6_OBJS = armv7/psarch.o   \

X86_OBJS = x86/psarch.o   \

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

