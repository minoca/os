################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Binary Name:
#
#       Kernel
#
#   Abstract:
#
#       This is the core of the operating system.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = kernel

BINPLACE = bin

BINARYTYPE = so

##
## Link in all object files of the included libraries as they are exported.
##

EXTRA_LDFLAGS += -Wl,--whole-archive

ENTRY = KepStartSystem

ifeq (armv7, $(ARCH))
ARCHLIB = $(INTLIB)/os/kernel/armv7/armsup.a
endif

ifeq (armv6, $(ARCH))
ARCHLIB = $(INTLIB)/os/kernel/armv6/armsup.a
endif

ifeq (x86, $(ARCH))
ARCHLIB = $(INTLIB)/os/kernel/x86/x86sup.a
endif

TARGETLIBS = $(INTLIB)/$(THISDIR)/acpi/acpi.a           \
             $(INTLIB)/os/lib/crypto/crypto.a           \
             $(INTLIB)/$(THISDIR)/ob/ob.a               \
             $(INTLIB)/os/lib/rtl/base/basertl.a        \
             $(INTLIB)/os/lib/rtl/kmode/krtl.a          \
             $(INTLIB)/os/lib/im/im.a                   \
             $(INTLIB)/os/lib/basevid/basevid.a         \
             $(INTLIB)/os/lib/termlib/termlib.a         \
             $(INTLIB)/$(THISDIR)/kd/kd.a               \
             $(INTLIB)/$(THISDIR)/kd/kdusb/kdusb.a      \
             $(INTLIB)/$(THISDIR)/ps/ps.a               \
             $(INTLIB)/$(THISDIR)/ke/ke.a               \
             $(INTLIB)/$(THISDIR)/io/io.a               \
             $(INTLIB)/$(THISDIR)/hl/hl.a               \
             $(INTLIB)/$(THISDIR)/mm/mm.a               \
             $(INTLIB)/$(THISDIR)/sp/sp.a               \
             $(ARCHLIB)                                 \

OBJS = init.o

DIRS = mm    \
       ob    \
       acpi  \
       hl    \
       io    \
       ke    \
       ps    \
       x86   \
       armv7 \
       armv6 \
       kd    \
       sp    \

include $(SRCROOT)/os/minoca.mk

$(INTLIB)/$(THISDIR)/acpi/acpi.a: acpi
$(INTLIB)/$(THISDIR)/ob/ob.a: ob
$(INTLIB)/$(THISDIR)/kd/kd.a: kd
$(INTLIB)/$(THISDIR)/kd/kdusb/kdusb.a: kd
$(INTLIB)/$(THISDIR)/ps/ps.a: ps
$(INTLIB)/$(THISDIR)/ke/ke.a: ke
$(INTLIB)/$(THISDIR)/io/io.a: io
$(INTLIB)/$(THISDIR)/hl/hl.a: hl
$(INTLIB)/$(THISDIR)/mm/mm.a: mm
$(INTLIB)/$(THISDIR)/sp/sp.a: sp
$(ARCHLIB): armv7 armv6 x86

VPATH += $(BINROOT)

postbuild: $(BINROOT)/dev2drv.set $(BINROOT)/devmap.set $(BINROOT)/init.set
postbuild: $(BINROOT)/init.sh

$(BINROOT)/dev2drv.set: config/dev2drv.set
	@cp -pv $^ $@

$(BINROOT)/devmap.set: config/devmap.set.$(ARCH)
	@cp -pv $^ $@

$(BINROOT)/init.set: config/init.set
	@cp -pv $^ $@

$(BINROOT)/init.sh: config/init.sh
	@cp -pv $^ $@

