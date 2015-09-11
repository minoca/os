################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Input/Output
#
#   Abstract:
#
#       This library contains the I/O (Input/Output) functionality of the
#       kernel. The I/O library manages devices, connects them to drivers, and
#       coordinates exposing their functionality to other kernel components and
#       user mode.
#
#   Author:
#
#       Evan Green 16-Sep-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = io.a

BINARYTYPE = library

BINPLACE = intlib

OBJS = arb.o      \
       cachedio.o \
       device.o   \
       devinfo.o  \
       devrem.o   \
       devres.o   \
       driver.o   \
       fileobj.o  \
       filesys.o  \
       flock.o    \
       info.o     \
       init.o     \
       intrface.o \
       intrupt.o  \
       iobase.o   \
       iohandle.o \
       irp.o      \
       mount.o    \
       obfs.o     \
       pagecach.o \
       path.o     \
       perm.o     \
       pipe.o     \
       pminfo.o   \
       power.o    \
       pstate.o   \
       pty.o      \
       pwropt.o   \
       shmemobj.o \
       socket.o   \
       stream.o   \
       testhook.o \
       unsocket.o \
       userio.o   \

ARMV7_OBJS = armv7/archio.o   \

ARMV6_OBJS = armv7/archio.o   \

X86_OBJS = x86/archio.o   \

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

