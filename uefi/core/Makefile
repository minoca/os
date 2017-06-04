################################################################################
#
#   Copyright (c) 2014 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Core
#
#   Abstract:
#
#       This file is responsible for building the core UEFI support. This is a
#       library containing the bulk of the UEFI interfaces, which platform
#       specific images include into their boot image.
#
#   Author:
#
#       Evan Green 26-Feb-2014
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY = ueficore.a

BINARYTYPE = klibrary

INCLUDES += $(SRCROOT)/os/uefi/include;.;

VERSION_MAJOR := 1
VERSION_MINOR := 0
VERSION_REVISION := 0
VERSION_RELEASE := SystemReleaseFinal

OBJS = acpi.o     \
       acpitabs.o \
       basepe.o   \
       bdsboot.o  \
       bdscon.o   \
       bdsentry.o \
       bdsutil.o  \
       cfgtable.o \
       dbgser.o   \
       devpathu.o \
       diskio.o   \
       dispatch.o \
       div.o      \
       drvsup.o   \
       event.o    \
       fatdev.o   \
       fatfs.o    \
       fsvars.o   \
       fvblock.o  \
       fvsect.o   \
       fwvol.o    \
       fwvolio.o  \
       handle.o   \
       image.o    \
       init.o     \
       intr.o     \
       locate.o   \
       lock.o     \
       memory.o   \
       part.o     \
       partelto.o \
       partgpt.o  \
       partmbr.o  \
       pool.o     \
       ramdisk.o  \
       smbios.o   \
       stubs.o    \
       tpl.o      \
       timer.o    \
       util.o     \
       version.o  \
       vidcon.o   \

X86_OBJS = x86/archsup.o    \
           x86/prochw.o     \

ARMV7_OBJS = armv7/commsup.o    \
             armv7/inttable.o   \
             armv7/prochw.o     \

ARMV6_OBJS = armv7/commsup.o    \
             armv7/inttable.o   \
             armv7/prochw.o     \

EXTRA_SRC_DIRS = x86 armv7 armv6

DIRS = emptyrd  \
       rtlib    \
       runtime

include $(SRCROOT)/os/minoca.mk

CFLAGS += -fshort-wchar

##
## Generate the version header.
##

version.o: $(CURDIR)/version.h

$(CURDIR)/version.h:
	$(GEN_VERSION) $@ header $(VERSION_MAJOR) $(VERSION_MINOR) \
	    $(VERSION_REVISION) $(VERSION_RELEASE)

