################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

EXTRA_CFLAGS += -ffreestanding

OBJS = bsearch.o           \
       bsrchtst.o          \
       getopt.o            \
       getoptst.o          \
       abs.o               \
       ceil.o              \
       exp.o               \
       expm1.o             \
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

