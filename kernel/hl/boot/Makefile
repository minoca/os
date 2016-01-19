################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Hardware Layer (Boot)
#
#   Abstract:
#
#       This directory builds a portion of the Hardware Layer Library for use
#       in the boot environment.
#
#   Author:
#
#       Evan Green 8-Aug-2013
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = hlboot.a

BINARYTYPE = library

INCLUDES += $(SRCDIR)/..;

OBJS = hmodapi.o     \
       ../dbgdev.o   \

ARMV7_OBJS = ../armv7/archdbg.o  \
             ../armv7/regacces.o \
             ../armv7/uartpl11.o \
             ../armv7/uartomap.o \
             ../ns16550.o        \

ARMV6_OBJS = ../armv6/archdbg.o  \
             ../armv7/regacces.o \
             ../armv7/uartpl11.o \

X86_OBJS = ../x86/archdbg.o  \
           ../x86/ioport.o   \
           ../x86/regacces.o \
           ../ns16550.o      \

include $(SRCROOT)/os/minoca.mk

