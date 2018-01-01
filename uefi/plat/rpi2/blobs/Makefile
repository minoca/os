################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       Blobs
#
#   Abstract:
#
#       This directory contains the binary blobs needed to boot the Raspberry
#       Pi and Raspberry Pi 2.
#
#   Author:
#
#       Evan Green 26-Oct-2015
#
#   Environment:
#
#       User Mode
#
################################################################################

include $(SRCROOT)/os/minoca.mk

BINDIR = $(BINROOT)/rpi

BINFILES = $(BINDIR)/bootcode.bin     \
           $(BINDIR)/config.txt       \
           $(BINDIR)/fixup.dat        \
           $(BINDIR)/fixup_cd.dat     \
           $(BINDIR)/fixup_db.dat     \
           $(BINDIR)/fixup_x.dat      \
           $(BINDIR)/start.elf        \
           $(BINDIR)/start_cd.elf     \
           $(BINDIR)/start_db.elf     \
           $(BINDIR)/start_x.elf      \
           $(BINDIR)/LICENCE.broadcom \

all: $(BINFILES)

$(BINFILES): | $(BINDIR)

$(BINDIR):
	@mkdir -p $@

$(BINDIR)/bootcode.bin: bootcode.bin
	@cp -pv $< $@

$(BINDIR)/config.txt: config.txt
	@cp -pv $< $@

$(BINDIR)/fixup.dat: fixup.dat
	@cp -pv $< $@

$(BINDIR)/fixup_cd.dat: fixup_cd.dat
	@cp -pv $< $@

$(BINDIR)/fixup_db.dat: fixup_db.dat
	@cp -pv $< $@

$(BINDIR)/fixup_x.dat: fixup_x.dat
	@cp -pv $< $@

$(BINDIR)/start.elf: start.elf
	@cp -pv $< $@

$(BINDIR)/start_cd.elf: start_cd.elf
	@cp -pv $< $@

$(BINDIR)/start_db.elf: start_db.elf
	@cp -pv $< $@

$(BINDIR)/start_x.elf: start_x.elf
	@cp -pv $< $@

$(BINDIR)/LICENCE.broadcom: LICENCE.broadcom
	@cp -pv $< $@
