################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       C Library Test
#
#   Abstract:
#
#       This program tests the independent portions of the C library.
#
#   Author:
#
#       Evan Green 9-Jul-2013
#
#   Environment:
#
#       Test
#
################################################################################

BINARY = testc

BINARYTYPE = build

BUILD = yes

BINPLACE = testbin

INCLUDES += $(SRCROOT)/os/apps/libc/include;

VPATH += $(SRCDIR)/..:$(SRCDIR)/../math:

OBJS = bsearch.o           \
       bsrchtst.o          \
       getopt.o            \
       getoptst.o          \
       abs.o               \
       ceil.o              \
       exp.o               \
       floor.o             \
       fmod.o              \
       hypot.o             \
       log.o               \
       log2.o              \
       log10.o             \
       modf.o              \
       pow.o               \
       scalbn.o            \
       sqrt.o              \
       trig.o              \
       trigarc.o           \
       trighyp.o           \
       util.o              \
       mathtst.o           \
       qsort.o             \
       qsorttst.o          \
       regexcmp.o          \
       regexexe.o          \
       regextst.o          \
       testc.o             \

include $(SRCROOT)/os/minoca.mk

