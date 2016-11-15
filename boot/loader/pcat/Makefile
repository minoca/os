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
#       PC/AT Loader
#
#   Abstract:
#
#       This module implements the operating system loader that loads the
#       kernel, targeted toward BIOS (non-UEFI) systems.
#
#   Author:
#
#       Evan Green 29-Jul-2012
#
#   Environment:
#
#       Boot
#
################################################################################

include $(SRCDIR)/../sources

VPATH += $(SRCDIR)/..:

OBJS += memory.o

INCLUDES += $(SRCROOT)/os/boot/lib/include;$(SRCDIR)/..;

TARGETLIBS += $(OBJROOT)/os/boot/lib/pcat/bootpcat.a  \
              $(OBJROOT)/os/lib/partlib/partlib.a     \

include $(SRCROOT)/os/minoca.mk

