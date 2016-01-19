################################################################################
#
#   Copyright (c) 2015 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Setup Configuration Files
#
#   Abstract:
#
#       This directory builds setup recipes into object files.
#
#   Author:
#
#       Evan Green 21-Oct-2015
#
#   Environment:
#
#       Setup
#
################################################################################

BINARY = msetplat.a

BINARYTYPE = library

include $(SRCDIR)/sources

DIRS = build

include $(SRCROOT)/os/minoca.mk

##
## Build .o files from .txt files.
##

%.o:%.txt
	@echo Objectifying - $(notdir $<)
	@cp $< $(CURDIR) && \
	   $(OBJCOPY) --rename-section \
	       .data=.rodata,alloc,load,data,contents,readonly -I binary \
	       -O $(OBJ_FORMAT) -B $(BFD_ARCH) $(notdir $<) $@ && \
	   rm $(notdir $<)

