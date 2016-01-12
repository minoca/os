################################################################################
#
#   Copyright (c) 2013 Minoca Corp. All Rights Reserved
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

BINPLACE = bin

BUILD = yes

BINARYTYPE = build

OBJS = tzcomp.o      \

TZDATA_DIR = $(SRCROOT)/os/tzcomp/data
TZ_FILES = $(TZDATA_DIR)/africa         \
           $(TZDATA_DIR)/antarctica     \
           $(TZDATA_DIR)/asia           \
           $(TZDATA_DIR)/australasia    \
           $(TZDATA_DIR)/etcetera       \
           $(TZDATA_DIR)/europe         \
           $(TZDATA_DIR)/leapseconds    \
           $(TZDATA_DIR)/northamerica   \
           $(TZDATA_DIR)/southamerica

TZ_DEFAULT = America/Los_Angeles

include $(SRCROOT)/os/minoca.mk

VPATH += $(BINROOT)

postbuild: tzdata tzdflt

tzdata: $(TZ_FILES) | $(BINARY)
	@echo Compiling time zone almanac
	@$(BINROOT)/$(BINARY) $(TZ_FILES) -o $(BINROOT)/tzdata

tzdflt: $(TZ_FILES) | $(BINARY)
	@echo Compiling default time zone: $(TZ_DEFAULT)
	@$(BINROOT)/$(BINARY) $(TZ_FILES) -o $(BINROOT)/tzdflt -f $(TZ_DEFAULT)

