################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       PC/AT Boot Library
#
#   Abstract:
#
#       This library contains the PC/AT (BIOS) Boot Library.
#
#   Author:
#
#       Evan Green 19-Feb-2014
#
#   Environment:
#
#       Boot
#
################################################################################

BINARY = bootpcat.a

BINARYTYPE = library

BINPLACE = intlib

include ../sources

INCLUDES += $(SRCDIR)/../inc;$(SRCDIR)/..;

OBJS += fwapi.o      \
        int10.o      \
        int13.o      \
        memory.o     \
        realmexe.o   \
        realmode.o   \
        reset.o      \
        rsdp.o       \
        time.o       \

include $(SRCROOT)/os/minoca.mk

