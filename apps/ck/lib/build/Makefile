################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk Library (Build)
#
#   Abstract:
#
#       This library contains the Chalk interpreter targeted toward the build
#       machine environment.
#
#   Author:
#
#       Evan Green 9-Jun-2016
#
#   Environment:
#
#       POSIX
#
################################################################################

BINARY = chalk.a

BINARYTYPE = library

BUILD = yes

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

OBJS += dynwin32.o \

else

OBJS += dlopen.o \

endif

include $(SRCROOT)/os/minoca.mk

gram.c:
	@echo Generating Grammar - $@
	@../gram/gramgen $@

##
## Build .o files from .ck sources.
##

ifeq ($(OS),Windows_NT)

EXTRA_OBJCOPY_ARGS = --prefix-symbols=_

endif

%.o:%.ck
	@echo Objectifying - $(notdir $<)
	@cp $< $(CURDIR) && \
	   $(OBJCOPY) --rename-section \
	       .data=.rodata,alloc,load,data,contents,readonly -I binary \
	       -O $(OBJ_FORMAT) -B $(BFD_ARCH) $(EXTRA_OBJCOPY_ARGS) \
	       $(notdir $<) $@ && \
	   rm $(notdir $<)

