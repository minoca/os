################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       System Profiler
#
#   Abstract:
#
#       This library contains the System Profiler, which lends insight into the
#       real-time resource usage of the system.
#
#   Author:
#
#       Chris Stevens 1-Jul-2013
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = sp.a

BINARYTYPE = library

OBJS = info.o \
       profiler.o \

X86_OBJS = x86/archprof.o \

ARMV7_OBJS = armv7/archprof.o \

ARMV6_OBJS = armv7/archprof.o \

EXTRA_SRC_DIRS = x86 armv7

include $(SRCROOT)/os/minoca.mk

