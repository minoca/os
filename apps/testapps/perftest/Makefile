################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Performance Benchmark Test
#
#   Abstract:
#
#       This executable implements the performance benchmark test application.
#
#   Author:
#
#       Chris Stevens 27-Apr-2015
#
#   Environment:
#
#       User Mode
#
################################################################################

BINARY = perftest

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/libc/include;

OBJS = copy.o     \
       create.o   \
       dlopen.o   \
       dup.o      \
       getppid.o  \
       exec.o     \
       fork.o     \
       malloc.o   \
       mmap.o     \
       mutex.o    \
       open.o     \
       perfsup.o  \
       perftest.o \
       pipeio.o   \
       pthread.o  \
       read.o     \
       rename.o   \
       signal.o   \
       stat.o     \
       write.o    \

DIRS = perflib

include $(SRCROOT)/os/minoca.mk

