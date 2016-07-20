################################################################################
#
#   Copyright (c) 2016 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Chalk
#
#   Abstract:
#
#       This library contains the Chalk interpreter, which provides a
#       basic programming language.
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

include $(SRCDIR)/sources

DIRS = build \
       gram

include $(SRCROOT)/os/minoca.mk

build: gram
gram.c: gram/gramgen.c
	@echo Generating Grammar - $@
	@./gram/gramgen $@

##
## Objectify the .ck sources into data that gets compiled at runtime.
##

%.o:%.ck
	@echo Objectifying - $(notdir $<)
	@cp $< $(CURDIR) && \
	   $(OBJCOPY) --rename-section \
	       .data=.rodata,alloc,load,data,contents,readonly -I binary \
	       -O $(OBJ_FORMAT) -B $(BFD_ARCH) $(notdir $<) $@ && \
	   rm $(notdir $<)
