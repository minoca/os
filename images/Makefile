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
#       Images
#
#   Abstract:
#
#       This module builds the final OS images.
#
#   Author:
#
#       Evan Green 26-Oct-2015
#
#   Environment:
#
#       Build
#
################################################################################

include $(SRCROOT)/os/minoca.mk

VPATH += $(BINROOT):

INSTALL_IMAGE := $(BINROOT)/install.img

IMAGES = $(INSTALL_IMAGE)

ifneq ($(DEBUG),rel)
MSETUP_DEBUG := -D
endif

MSETUP := msetup
MSETUP_IMAGE_SIZE := -G1024M
MSETUP_TINY_IMAGE_SIZE := -G30M

##
## Build x86 images.
##

ifeq (x86, $(ARCH)$(VARIANT))

IMAGES += $(BINROOT)/pc.img \
          $(BINROOT)/pcefi.img \
          $(BINROOT)/pctiny.img \

$(INSTALL_IMAGE): $(LAST_UPDATE_FILE)
	@echo Building - $(notdir $@)
	@mkdir -p $(BINROOT)/apps
	@cd $(BINROOT) && $(MSETUP) -ql install-x86 -i.. -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/pc.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql pc -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/pcefi.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql pcefi -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/pctiny.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql pc-tiny -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_TINY_IMAGE_SIZE)

endif

##
## Build x64 images.
## TODO: Set platform back to pc and add pcefi once x64 compiles to match x86.
##

ifeq (x64, $(ARCH)$(VARIANT))

IMAGES += $(BINROOT)/pc.img \

$(INSTALL_IMAGE): $(LAST_UPDATE_FILE)
	@echo Building - $(notdir $@)
	@mkdir -p $(BINROOT)/apps
	@cd $(BINROOT) && $(MSETUP) -ql install-x64 -i.. -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/pc.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql pc64 -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

endif

##
## Build x86 Quark images. Debugging is currently disabled on Galileo since the
## headphones UART is used for the console.
##

ifeq (x86q, $(ARCH)$(VARIANT))

IMAGES += $(BINROOT)/galileo.img

$(INSTALL_IMAGE): $(LAST_UPDATE_FILE)
	@echo Building - $(notdir $@)
	@mkdir -p $(BINROOT)/apps
	@cd $(BINROOT) && $(MSETUP) -ql install-x86 -i.. -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/galileo.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql galileo -i $< -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

endif

##
## Build ARMv7 images.
##

ifeq (armv7, $(ARCH))

IMAGES += $(BINROOT)/bbone.img \
          $(BINROOT)/integ.img \
          $(BINROOT)/integrd.img \
          $(BINROOT)/panda.img \
          $(BINROOT)/pandausb.img \
          $(BINROOT)/rpi2.img \
          $(BINROOT)/veyron.img \

$(INSTALL_IMAGE): $(LAST_UPDATE_FILE)
	@echo Building - $(notdir $@)
	@mkdir -p $(BINROOT)/apps
	@cd $(BINROOT) && $(MSETUP) -ql install-armv7 -i.. -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/bbone.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql beagleboneblack -i $< $(MSETUP_DEBUG) \
	    -d $@ -a0 $(MSETUP_IMAGE_SIZE)

$(BINROOT)/integ.img: $(BINROOT)/integrd.img
	@echo Building - $(notdir $@)
	@cp $^ $(OBJROOT)/os/uefi/plat/integcp/ramdisk
	@cd $(SRCROOT)/os/uefi/plat/integcp && \
	    $(MAKE) BINARY=integ.img SRCDIR=$(SRCROOT)/os/uefi/plat/integcp

$(BINROOT)/integrd.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql integrd -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_TINY_IMAGE_SIZE)

$(BINROOT)/panda.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql panda -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/pandausb.img: $(BINROOT)/pandard.img
	@echo Building - $(notdir $@)
	@cp $^ $(OBJROOT)/os/uefi/plat/panda/ramdisk
	@cd $(SRCROOT)/os/uefi/plat/panda && \
	    $(MAKE) BINARY=pandafw_usb.elf SRCDIR=$(SRCROOT)/os/uefi/plat/panda

$(BINROOT)/pandard.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql panda-usb -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_TINY_IMAGE_SIZE)

$(BINROOT)/rpi2.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql raspberrypi2 -i $< $(MSETUP_DEBUG) -d $@ \
	    -a0 $(MSETUP_IMAGE_SIZE)

$(BINROOT)/veyron.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql veyron -i $< $(MSETUP_DEBUG) -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

endif

##
## Build ARMv6 images.
##

ifeq (armv6, $(ARCH))

IMAGES += $(BINROOT)/rpi.img \

$(INSTALL_IMAGE): $(LAST_UPDATE_FILE)
	@echo Building - $(notdir $@)
	@mkdir -p $(BINROOT)/apps
	@cd $(BINROOT) && $(MSETUP) -ql install-armv6 -i.. -d $@ -a0 \
	    $(MSETUP_IMAGE_SIZE)

$(BINROOT)/rpi.img: $(INSTALL_IMAGE)
	@echo Building - $(notdir $@)
	@cd $(BINROOT) && $(MSETUP) -ql raspberrypi -i $< $(MSETUP_DEBUG) -d $@ \
	    -a0 $(MSETUP_IMAGE_SIZE)

endif

all: $(IMAGES)
clean: images-clean

.PHONY: images-clean

images-clean:
	rm -f $(IMAGES)

