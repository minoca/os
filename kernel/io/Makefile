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

BINARYTYPE = klibrary

OBJS = arb.o      \
       cachedio.o \
       cstate.o   \
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
             armv7/archpm.o   \

ARMV6_OBJS = $(ARMV7_OBJS)

X86_OBJS = x86/archio.o   \
           x86/archpm.o   \
           x86/intelcst.o \

X64_OBJS = $(X86_OBJS)

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

