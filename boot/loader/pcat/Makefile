################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       PC/AT Loader
#
#   Abstract:
#
#       This module implements the operating system loader that loads the
#       kernel, targeted toward BIOS (non-UEFI) systems.
#
#   Author:
#
#       Evan Green 29-Jul-2012
#
#   Environment:
#
#       Boot
#
################################################################################

include $(SRCDIR)/../sources

VPATH += $(SRCDIR)/..:

OBJS += memory.o

INCLUDES += $(SRCROOT)/os/boot/lib/inc;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/bootpcat.a  \
              $(OBJROOT)/os/lib/partlib/partlib.a     \

include $(SRCROOT)/os/minoca.mk

