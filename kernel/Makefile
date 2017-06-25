################################################################################
#
#   Copyright (c) 2012 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
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

BINARYTYPE = driver

##
## Link in all object files of the included libraries as they are exported.
##

EXTRA_LDFLAGS += -Wl,--whole-archive

ENTRY = KepStartSystem

ifeq (armv7, $(ARCH))
ARCHLIB = $(OBJROOT)/os/kernel/armv7/armsup.a
endif

ifeq (armv6, $(ARCH))
ARCHLIB = $(OBJROOT)/os/kernel/armv6/armsup.a
endif

ifeq (x86, $(ARCH))
ARCHLIB = $(OBJROOT)/os/kernel/x86/x86sup.a
endif

ifeq (x64, $(ARCH))
ARCHLIB = $(OBJROOT)/os/kernel/x64/x64sup.a
endif

TARGETLIBS = $(OBJROOT)/$(THISDIR)/acpi/acpi.a           \
             $(OBJROOT)/os/lib/crypto/crypto.a           \
             $(OBJROOT)/$(THISDIR)/ob/ob.a               \
             $(OBJROOT)/os/lib/rtl/base/basertl.a        \
             $(OBJROOT)/os/lib/rtl/kmode/krtl.a          \
             $(OBJROOT)/os/lib/im/native/imn.a           \
             $(OBJROOT)/os/lib/basevid/basevid.a         \
             $(OBJROOT)/os/lib/termlib/termlib.a         \
             $(OBJROOT)/$(THISDIR)/kd/kd.a               \
             $(OBJROOT)/$(THISDIR)/kd/kdusb/kdusb.a      \
             $(OBJROOT)/$(THISDIR)/ps/ps.a               \
             $(OBJROOT)/$(THISDIR)/ke/ke.a               \
             $(OBJROOT)/$(THISDIR)/io/io.a               \
             $(OBJROOT)/$(THISDIR)/hl/hl.a               \
             $(OBJROOT)/$(THISDIR)/mm/mm.a               \
             $(OBJROOT)/$(THISDIR)/sp/sp.a               \
             $(ARCHLIB)                                  \

OBJS = init.o

DIRS = mm    \
       ob    \
       acpi  \
       hl    \
       io    \
       ke    \
       ps    \
       x86   \
       x64   \
       armv7 \
       armv6 \
       kd    \
       sp    \

include $(SRCROOT)/os/minoca.mk

$(OBJROOT)/$(THISDIR)/acpi/acpi.a: acpi
$(OBJROOT)/$(THISDIR)/ob/ob.a: ob
$(OBJROOT)/$(THISDIR)/kd/kd.a: kd
$(OBJROOT)/$(THISDIR)/kd/kdusb/kdusb.a: kd
$(OBJROOT)/$(THISDIR)/ps/ps.a: ps
$(OBJROOT)/$(THISDIR)/ke/ke.a: ke
$(OBJROOT)/$(THISDIR)/io/io.a: io
$(OBJROOT)/$(THISDIR)/hl/hl.a: hl
$(OBJROOT)/$(THISDIR)/mm/mm.a: mm
$(OBJROOT)/$(THISDIR)/sp/sp.a: sp
$(ARCHLIB): armv7 armv6 x86 x64

VPATH += $(BINROOT)

postbuild: $(BINROOT)/dev2drv.set $(BINROOT)/devmap.set $(BINROOT)/init.set
postbuild: $(BINROOT)/init.sh

$(BINROOT)/dev2drv.set: config/dev2drv.set
	@cp -pv $^ $@

$(BINROOT)/devmap.set: config/devmap.set
	@cp -pv $^ $@

$(BINROOT)/init.set: config/init.set
	@cp -pv $^ $@

$(BINROOT)/init.sh: config/init.sh
	@cp -pv $^ $@

