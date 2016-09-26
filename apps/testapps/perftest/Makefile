################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
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
       stat.o     \
       write.o    \

DYNLIBS = -lminocaos

DIRS = perflib

include $(SRCROOT)/os/minoca.mk

