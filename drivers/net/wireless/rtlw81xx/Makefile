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
#       RTLW 81xx
#
#   Abstract:
#
#       This module implements support for the Realtek RTL81xx family of
#       wireless LAN controllers.
#
#   Author:
#
#       Chris Stevens 10-Oct-2015
#
#   Environment:
#
#       Kernel
#
################################################################################

BINARY = rtlw81xx.drv

BINARYTYPE = driver

BINPLACE = bin

OBJS = rtlw81.o   \
       rtlw81hw.o \

DYNLIBS = $(BINROOT)/kernel                 \
          $(BINROOT)/netcore.drv            \
          $(BINROOT)/net80211.drv           \
          $(BINROOT)/usbcore.drv            \

include $(SRCROOT)/os/minoca.mk

postbuild: $(BINROOT)/rtlw8188eufw.bin $(BINROOT)/rtlw8188cufwUMC.bin
postbuild: $(BINROOT)/rtlw8192cufw.bin

$(BINROOT)/rtlw8188eufw.bin: firmware/rtlw8188eufw.bin
	@cp -pv $^ $@

$(BINROOT)/rtlw8188cufwUMC.bin: firmware/rtlw8188cufwUMC.bin
	@cp -pv $^ $@

$(BINROOT)/rtlw8192cufw.bin: firmware/rtlw8192cufw.bin
	@cp -pv $^ $@

