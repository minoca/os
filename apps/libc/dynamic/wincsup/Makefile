################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved.
#
#   Module Name:
#
#       Win32 C Library Support
#
#   Abstract:
#
#       This module compiles a subset of the Minoca C library retargetd towards
#       Windows platforms.
#
#   Author:
#
#       Evan Green 10-Sep-2013
#
#   Environment:
#
#       Kernel Mode
#
################################################################################

BINARY = wincsup.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/include;..;

VPATH += $(SRCDIR)/..:

X86_OBJS = regexcmp.o       \
           regexexe.o       \
           strftime.o       \

include $(SRCROOT)/os/minoca.mk

##
## Override the compiler since this is a Windows library.
##

CC := gcc
LD := ld
RCC := windres
AR := ar
AS := as

