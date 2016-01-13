################################################################################
#
#   Copyright (c) 2014 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       efiboot
#
#   Abstract:
#
#       This executable implements support for the efiboot program, which is a
#       user mode utility that can be used to change EFI boot options.
#
#   Author:
#
#       Evan Green 9-Dec-2014
#
#   Environment:
#
#       User
#
################################################################################

BINARY = efiboot

BINPLACE = bin

BINARYTYPE = app

INCLUDES += $(SRCROOT)/os/apps/inc;      \
            $(SRCROOT)/os/apps/inc/libc; \

OBJS = efiboot.o \

CFLAGS += -fshort-wchar

ifeq ($(ARCH), $(filter armv7 armv6,$(ARCH)))
LDFLAGS += -Wl,--no-wchar-size-warning
endif

DYNLIBS = -lminocaos

include $(SRCROOT)/os/minoca.mk

