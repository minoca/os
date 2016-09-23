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

BINARY := libchalk.so.1

BINARYTYPE = so

BUILD = yes

##
## TODO: The build and native versions of these libraries collide in the bin
## directory. The new build infrastructure should address this.
##

BINPLACE = bin

INCLUDES += $(SRCDIR)/..;

VPATH += $(SRCDIR)/..:

ENTRY := 0

include $(SRCDIR)/../sources

ifeq ($(OS),Windows_NT)

BINARY := chalk.dll
BINARYTYPE := dll

OBJS += $(WIN32_OBJS)

else

OBJS += $(POSIX_OBJS)

endif

TARGETLIBS = $(OBJROOT)/os/lib/yy/build/yy.a                     \
             $(OBJROOT)/os/lib/rtl/base/build/basertl.a          \
             $(OBJROOT)/os/lib/rtl/rtlc/build/rtlc.a             \

ifneq ($(OS),$(filter Windows_NT Minoca,$(OS)))

DYNLIBS += -ldl

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
	@[ -r $(notdir $<) ] || cp $< $(CURDIR) && \
	   $(OBJCOPY) --rename-section \
	       .data=.rodata,alloc,load,data,contents,readonly -I binary \
	       -O $(OBJ_FORMAT) -B $(BFD_ARCH) $(EXTRA_OBJCOPY_ARGS) \
	       $(notdir $<) $@ && \
	   rm $(notdir $<)

