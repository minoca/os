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

BINARYTYPE = klibrary

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
       uts.o      \

ARMV7_OBJS = armv7/psarch.o   \

ARMV6_OBJS = armv7/psarch.o   \

X86_OBJS = x86/psarch.o   \

X64_OBJS = x64/psarch.o   \

EXTRA_SRC_DIRS = x86 x64 armv7

include $(SRCROOT)/os/minoca.mk

