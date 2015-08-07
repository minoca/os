################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       CreateImage
#
#   Abstract:
#
#       This module builds the createimage executable, which can build flat
#       ready-to-boot disk images.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = createimage

BINPLACE = bin

BUILD = yes

BINARYTYPE = build

TARGETLIBS = $(INTLIB)/os/lib/rtl/base/build/basertl.a      \
             $(INTLIB)/os/lib/rtl/build/rtlstubs.a          \
             $(INTLIB)/os/lib/fatlib/build/fat.a            \
             $(INTLIB)/os/lib/partlib/build/partlib.a       \
             $(INTLIB)/os/lib/bconflib/build/bconflib.a     \

OBJS = cibconf.o     \
       cipart.o      \
       createimage.o \
       fatdev.o      \

include $(SRCROOT)/os/minoca.mk

VPATH += $(BINROOT):

export STRIP
export BINROOT

##
## Perform x86 postbuild commands to create the final images.
##

ifeq (x86, $(ARCH))

postbuild:
	@cd $(BINROOT) && \
	 sh $(SRCROOT)/os/tasks/images/build_install.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_pc.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_tinydemo.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_pcefi.sh

endif

##
## Perform ARMv7 postbuild commands to create ARMv7 images.
##

ifeq (armv7, $(ARCH))

export OBJROOT
export OBJCOPY

postbuild:
	@cd $(BINROOT) && \
	 sh $(SRCROOT)/os/tasks/images/build_armefi.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_panda.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_pandausb.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_bbone.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_rpi2.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_integ.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_veyron.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_install.sh

endif

##
## Perform ARMv6 postbuild commands to create ARMv6 images.
##

ifeq (armv6, $(ARCH))

export OBJROOT
export OBJCOPY

postbuild:
	@cd $(BINROOT) && \
	 sh $(SRCROOT)/os/tasks/images/build_rpi.sh && \
	 sh $(SRCROOT)/os/tasks/images/build_install.sh

endif

