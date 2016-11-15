################################################################################
#
#   Copyright (c) 2015 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Module Name:
#
#       Veyron ACPI tables
#
#   Abstract:
#
#       This module compiles the Veyron ACPI tables.
#
#   Author:
#
#       Evan Green 10-Jul-2015
#
#   Environment:
#
#       Firmware
#
################################################################################

BINARY := acpi.ffs

BINARYTYPE = none

IASL = iasl

IASL_OPTIONS = -we

OBJS += apic.aml     \
        dbg2.aml     \
        dsdt.aml     \
        facp.aml     \
        facs.aml     \
        gtdt.aml     \
        rk32.aml     \

include $(SRCROOT)/os/minoca.mk

$(BINARY): $(OBJS)
	@echo Generating - $@
	@cd $(OBJROOT)/$(THISDIR) && \
	 genffs -g 7E374E25-8E01-4FEE-87F2-390C23C606CD -r EFI_SECTION_RAW \
	 -t EFI_FV_FILETYPE_FREEFORM -o $@ $^

%.aml:%.asl
	@echo Compiling ASL - $<
	@$(IASL) $(IASL_OPTIONS) -p $(OBJROOT)/$(THISDIR)/$@ $< > $(OBJROOT)/$(THISDIR)/iaslout.txt

