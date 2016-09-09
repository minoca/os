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

BINARY = libchalk.so.1

BINARYTYPE = so

BINPLACE = bin

include $(SRCDIR)/sources

OBJS += $(POSIX_OBJS)

DIRS = build \
       gram

ENTRY := 0

TARGETLIBS = $(OBJROOT)/os/lib/rtl/base/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/rtlc.a             \
             $(OBJROOT)/os/lib/yy/yy.a                     \

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

