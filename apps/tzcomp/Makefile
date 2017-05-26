################################################################################
#
#   Copyright (c) 2013 Minoca Corp.
#
#    This file is licensed under the terms of the GNU General Public License
#    version 3. Alternative licensing terms are available. Contact
#    info@minocacorp.com for details. See the LICENSE file at the root of this
#    project for complete licensing information.
#
#   Binary Name:
#
#       TzComp
#
#   Abstract:
#
#       This executable compiles textual time zone data into a binary format.
#
#   Author:
#
#       Evan Green 2-Aug-2013
#
#   Environment:
#
#       Build
#
################################################################################

BINARY = tzcomp

BINPLACE = tools/bin

BUILD = yes

BINARYTYPE = build

OBJS = tzcomp.o      \

TZDATA_DIR = $(SRCROOT)/os/apps/tzcomp/data
TZ_FILES = $(TZDATA_DIR)/africa         \
           $(TZDATA_DIR)/antarctica     \
           $(TZDATA_DIR)/asia           \
           $(TZDATA_DIR)/australasia    \
           $(TZDATA_DIR)/etcetera       \
           $(TZDATA_DIR)/europe         \
           $(TZDATA_DIR)/leapseconds    \
           $(TZDATA_DIR)/northamerica   \
           $(TZDATA_DIR)/southamerica

##
## Define the default time zone.
##

TZ_DEFAULT = America/Los_Angeles

##
## Define the cutoff year for time zone data.
##

TZ_CUTOFF_YEAR = 1980

include $(SRCROOT)/os/minoca.mk

VPATH += $(BINROOT)

postbuild: $(BINROOT)/skel/usr/share/tz/tzdata $(BINROOT)/skel/etc/tz

$(BINROOT)/skel/usr/share/tz/tzdata: $(TZ_FILES) | $(BINARY)
	@echo Compiling time zone almanac
	@mkdir -p $(BINROOT)/skel/usr/share/tz
	@./$(BINARY) $(TZ_FILES) -o $@ -y $(TZ_CUTOFF_YEAR)

$(BINROOT)/skel/etc/tz: $(TZ_FILES) | $(BINARY)
	@echo Compiling default time zone: $(TZ_DEFAULT)
	@mkdir -p $(BINROOT)/skel/etc
	@./$(BINARY) $(TZ_FILES) -o $@ -z $(TZ_DEFAULT) -y $(TZ_CUTOFF_YEAR)

