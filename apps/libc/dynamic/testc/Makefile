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
       ceilf.o             \
       exp.o               \
       expf.o              \
       exp2.o              \
       exp2f.o             \
       expm1.o             \
       expm1f.o            \
       floor.o             \
       floorf.o            \
       fmod.o              \
       fmodf.o             \
       hypot.o             \
       hypotf.o            \
       log.o               \
       logf.o              \
       log2.o              \
       log2f.o             \
       log10.o             \
       log10f.o            \
       minmax.o            \
       minmaxf.o           \
       modf.o              \
       modff.o             \
       pow.o               \
       powf.o              \
       scalbn.o            \
       scalbnf.o           \
       sqrt.o              \
       sqrtf.o             \
       trig.o              \
       trigf.o             \
       trigarc.o           \
       trigarcf.o          \
       trighyp.o           \
       trighypf.o          \
       trunc.o             \
       truncf.o            \
       util.o              \
       mathtst.o           \
       mathftst.o          \
       qsort.o             \
       qsorttst.o          \
       regexcmp.o          \
       regexexe.o          \
       regextst.o          \
       testc.o             \

include $(SRCROOT)/os/minoca.mk

