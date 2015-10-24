################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Setup Configuration Files (Build)
#
#   Abstract:
#
#       This directory builds setup recipes for the build environment.
#
#   Author:
#
#       Evan Green 21-Oct-2015
#
#   Environment:
#
#       User
#
################################################################################

BINARY = msetplat.a

BINARYTYPE = library

BINPLACE = intlib

BUILD = yes

BINPLACE = intlib

VPATH += $(SRCDIR)/..:

include $(SRCDIR)/../sources

include $(SRCROOT)/os/minoca.mk

ifeq ($(OS),Windows_NT)

EXTRA_OBJCOPY_ARGS = --prefix-symbols=_

endif

##
## Build .o files from .txt files.
##

%.o:%.txt
	@echo Objectifying - $(notdir $<)
	@cp $< $(CURDIR) && \
	   $(OBJCOPY) --rename-section \
	       .data=.rodata,alloc,load,data,contents,readonly -I binary \
	       -O $(OBJ_FORMAT) -B $(BFD_ARCH) $(EXTRA_OBJCOPY_ARGS) \
	       $(notdir $<) $@ && \
	   rm $(notdir $<)

