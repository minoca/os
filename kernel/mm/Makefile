################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Memory Manager
#
#   Abstract:
#
#       This library contains the Memory Management library used by the kernel.
#       It handles everything related to memory in the kernel, and meshes
#       closely with the I/O and process libraries to provide fast and
#       efficient management of memory resources.
#
#   Author:
#
#       Evan Green 27-Jul-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = mm.a

BINARYTYPE = library

OBJS = block.o    \
       imgsec.o   \
       info.o     \
       init.o     \
       invipi.o   \
       iobuf.o    \
       load.o     \
       mdl.o      \
       paging.o   \
       physical.o \
       kpools.o   \
       virtual.o  \
       fault.o    \

ARMV7_OBJS = armv7/archcomc.o \
             armv7/archsupc.o \
             armv7/flush.o    \
             armv7/mapping.o  \
             armv7/usermem.o  \

ARMV6_OBJS = armv6/archsupc.o \
             armv7/archcomc.o \
             armv7/flush.o    \
             armv7/mapping.o  \
             armv7/usermem.o  \

X86_OBJS = x86/archsupc.o \
           x86/flush.o    \
           x86/mapping.o  \
           x86/usermem.o  \

EXTRA_SRC_DIRS = x86 armv7 armv6

DIRS = boot

boot: $(BINARY)

TESTDIRS = testmm

include $(SRCROOT)/os/minoca.mk

