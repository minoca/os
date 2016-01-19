################################################################################
#
#   Copyright (c) 2012 Minoca Corp. All Rights Reserved
#
#   Module Name:
#
#       OS Project Makefile
#
#   Abstract:
#
#       This file implements the base Makefile that all other makefiles include.
#
#   Author:
#
#       Evan Green 26-Jul-2012
#
#   Environment:
#
#       Build
#
################################################################################

##
## Define the default target.
##

all:

##
## Don't let make use any builtin rules.
##

.SUFFIXES:

##
## Define the architecture and object of the build machine.
##

OS ?= $(shell uname -s)
BUILD_ARCH = $(shell uname -m)
ifeq ($(BUILD_ARCH), $(filter i686 i586,$(BUILD_ARCH)))
BUILD_ARCH := x86
BUILD_BFD_ARCH := i386
BUILD_OBJ_FORMAT := elf32-i386
ifeq ($(OS),Windows_NT)
BUILD_OBJ_FORMAT := pe-i386
endif

else ifeq ($(BUILD_ARCH), $(filter armv7 armv6,$(BUILD_ARCH)))
BUILD_BFD_ARCH := arm
BUILD_OBJ_FORMAT := elf32-littlearm
else ifeq ($(BUILD_ARCH), x86_64)
BUILD_ARCH := x64
BUILD_OBJ_FORMAT := elf64-x86-64
BUILD_BFD_ARCH := arm
else
$(error Unknown architecture $(BUILD_ARCH))
endif

##
## Define build locations.
##

SRCROOT := $(subst \,/,$(SRCROOT))
OUTROOT := $(SRCROOT)/$(ARCH)$(DEBUG)
BINROOT := $(OUTROOT)/bin
TESTBIN := $(OUTROOT)/testbin
OBJROOT := $(OUTROOT)/obj
STRIPPED_DIR := $(BINROOT)/stripped

CURDIR := $(subst \,/,$(CURDIR))

##
## If the current directory is not in the object root, then change to the
## object directory and re-run make from there. The condition is "if removing
## OBJROOT makes no change".
##

ifeq ($(CURDIR), $(subst $(OBJROOT),,$(CURDIR)))

THISDIR := $(subst $(SRCROOT)/,,$(CURDIR))
OBJDIR := $(OBJROOT)/$(THISDIR)
MAKETARGET = $(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile \
    -I$(CURDIR) $(MAKECMDGOALS) SRCDIR=$(CURDIR)

.PHONY: $(OBJDIR) clean wipe

$(OBJDIR):
	+@[ -d $@ ] || mkdir -p $@
	+@$(MAKETARGET)

clean:
	rm -rf $(OBJROOT)/$(THISDIR)

wipe:
	rm -rf $(OBJROOT)
	rm -rf $(BINROOT)
	rm -rf $(TESTBIN)

Makefile: ;
%.mk :: ;
% :: $(OBJDIR) ; @:

else

THISDIR := $(subst $(OBJROOT)/,,$(CURDIR))

##
## VPATH specifies which directories make should look in to find all files.
## Paths are separated by colons.
##

VPATH += :$(SRCDIR):

##
## Executable variables
##

CC_FOR_BUILD ?= gcc
AR_FOR_BUILD ?= ar
OBJCOPY_FOR_BUILD ?= objcopy
STRIP_FOR_BUILD ?= strip

ifeq ($(OS), $(filter Windows_NT Minoca,$(OS)))
CECHO_CYAN ?= cecho -fC
else
CECHO_CYAN ?= echo
endif

ifeq (x86, $(ARCH))
CC := i686-pc-minoca-gcc
RCC := windres
AR := i686-pc-minoca-ar
OBJCOPY := i686-pc-minoca-objcopy
STRIP := i686-pc-minoca-strip
BFD_ARCH := i386
OBJ_FORMAT := elf32-i386

ifneq ($(QUARK),)
CC := i586-pc-minoca-gcc
AR := i586-pc-minoca-ar
OBJCOPY := i586-pc-minoca-objcopy
STRIP := i586-pc-minoca-strip
endif

endif

ifeq (x64, $(ARCH))
CC := x86_64-pc-minoca-gcc
AR := x86_64-pc-minoca-ar
OBJCOPY := x86_64-pc-minoca-objcopy
STRIP := x86_64-pc-minoca-strip
BFD_ARCH := x86-64
OBJ_FORMAT := elf64-x86-64
endif

ifeq (armv7, $(ARCH))
CC := arm-none-minoca-gcc
AR := arm-none-minoca-ar
OBJCOPY := arm-none-minoca-objcopy
STRIP := arm-none-minoca-strip
BFD_ARCH := arm
OBJ_FORMAT := elf32-littlearm
endif

ifeq (armv6, $(ARCH))
CC := arm-none-minoca-gcc
AR := arm-none-minoca-ar
OBJCOPY := arm-none-minoca-objcopy
STRIP := arm-none-minoca-strip
BFD_ARCH := arm
OBJ_FORMAT := elf32-littlearm
endif

##
## These define versioning information for the code.
##

SYSTEM_VERSION_MAJOR := 0
SYSTEM_VERSION_MINOR := 1
SYSTEM_VERSION_REVISION := 0

INIT_REV := 1598fc5f1734f7d7ee01e014ee64e131601b78a7
REVISION_EXTRA ?= $(shell \
    if [ -f $(SRCROOT)/os/.git/refs/replace/$(INIT_REV) ] ; then \
        echo 1 ; \
    else echo 1000 ; \
    fi)

REVISION ?= $(shell \
    echo $$((`cd $(SRCROOT)/os/ && git rev-list --count HEAD` + \
             $(REVISION_EXTRA))))

ifeq ($(REVISION),)
REVISION := 0
endif

BUILD_TIME := $(shell echo $$((`date "+%s"` - 978307200)))
BUILD_TIME_STRING := "$(shell date "+%a %b %d %Y %H:%M:%S")"
BUILD_STRING := "($(USERNAME))"

##
## Define a file that gets touched to indicate that something has changed and
## images need to be rebuilt.
##

LAST_UPDATE_FILE :=  $(OBJROOT)/last-update
UPDATE_LAST_UPDATE := date > $(LAST_UPDATE_FILE)

##
## Includes directory.
##

INCLUDES += $(SRCROOT)/os/inc

##
## Define default CFLAGS if none were specified elsewhere.
##

CFLAGS ?= -Wall -Werror
ifeq ($(DEBUG), chk)
CFLAGS += -O1
else
CFLAGS += -O2 -Wno-unused-but-set-variable
endif

##
## Compiler flags:
##
## -fno-builtin              Don't recognize functions that do not begin with
##                           '__builtin__' as prefix.
##
## -fno-omit-frame-pointer   Always create a frame pointer so that things can be
##                           debugged.
##
## -mno-ms-bitfields         Honor the packed attribute.
##
## -g                        Build with DWARF debugging symbol information.
##
## -I ...                    Specifies a list of include directories.
##
## -c                        Compile or assemble, but do not link.
##
## -Wall                     Turn on all compiler warnings.
##
## -Werror                   Treat all warnings as errors.
##
## -D...=...                 Standard defines usable by all C files.
##
## -fvisibility=hidden       Don't export every single symbol by default.
##
## -save-temps               Save temporary files.
##

EXTRA_CPPFLAGS += -I $(subst ;, -I ,$(INCLUDES))                             \
                  -DSYSTEM_VERSION_MAJOR=$(SYSTEM_VERSION_MAJOR)             \
                  -DSYSTEM_VERSION_MINOR=$(SYSTEM_VERSION_MINOR)             \
                  -DSYSTEM_VERSION_REVISION=$(SYSTEM_VERSION_REVISION)       \
                  -DREVISION=$(REVISION)                                     \
                  -DBUILD_TIME_STRING=\"$(BUILD_TIME_STRING)\"               \
                  -DBUILD_TIME=$(BUILD_TIME)                                 \
                  -DBUILD_STRING=\"$(BUILD_STRING)\"                         \
                  -DBUILD_USER=\"$(USERNAME)\"                               \

ifeq ($(DEBUG), chk)
EXTRA_CPPFLAGS += -DDEBUG=1
endif

EXTRA_CPPFLAGS_FOR_BUILD := $(EXTRA_CPPFLAGS)

EXTRA_CFLAGS += -fno-builtin -fno-omit-frame-pointer -g -save-temps=obj \
                -ffunction-sections -fdata-sections -fvisibility=hidden

EXTRA_CFLAGS_FOR_BUILD := $(EXTRA_CFLAGS)

EXTRA_CFLAGS += -fvisibility=hidden

ifeq ($(BINARYTYPE), $(filter so app dll library,$(BINARYTYPE)))
EXTRA_CFLAGS += -fPIC
endif

##
## Restrict ARMv6 to armv6zk instructions to support the arm1176jzf-s.
##

ifeq (armv6, $(ARCH))
ifneq ($(BINARYTYPE), $(filter ntconsole win32 dll,$(BINARYTYPE)))
EXTRA_CPPFLAGS += -march=armv6zk -marm
endif
endif

ifeq (x86, $(ARCH))
EXTRA_CFLAGS += -mno-ms-bitfields

##
## Quark has an errata that requires no LOCK prefixes on instructions.
##

ifneq ($(QUARK),)
ifneq ($(BINARYTYPE), $(filter ntconsole win32 dll,$(BINARYTYPE)))
EXTRA_CPPFLAGS += -Wa,-momit-lock-prefix=yes -march=i586
endif
endif

ifeq ($(BINARYTYPE),app)
EXTRA_CFLAGS += -mno-stack-arg-probe
endif
endif

ifeq ($(BINARYTYPE),so)
ENTRY ?= DriverEntry
endif

##
## Build binaries on windows need a .exe suffix.
##

ifeq ($(OS),Windows_NT)
ifeq (x86, $(BUILD_ARCH))
EXTRA_CFLAGS_FOR_BUILD += -mno-ms-bitfields
endif
ifeq (build,$(BINARYTYPE))
BINARY := $(BINARY).exe
endif
endif

ENTRY ?= _start

##
## Linker flags:
##
## -T linker_script          Specifies a custom linker script.
##
## -Ttext X                  Use X as the starting address for the text section
##                           of the output file. One of the defined addresses
##                           above will get placed after the linker options, so
##                           this option MUST be last.
##
## -e <symbol>               Sets the entry point of the binary to the given
##                           symbol.
##
## -u <symbol>               Start with an undefined reference to the entry
##                           point, in case it is in a static library.
##
## -Map                      Create a linker map for reference as well.
##

ifneq (,$(TEXT_ADDRESS))
EXTRA_LDFLAGS += -Wl,-Ttext-segment=$(TEXT_ADDRESS) -Wl,-Ttext=$(TEXT_ADDRESS)
endif

ifneq (,$(LINKER_SCRIPT))
EXTRA_LDFLAGS += -T$(LINKER_SCRIPT)
endif

EXTRA_LDFLAGS += -Wl,-e$(ENTRY)                             \
                 -Wl,-u$(ENTRY)                             \
                 -Wl,-Map=$(BINARY).map                     \
                 -Wl,--gc-sections                          \

ifeq ($(BINARYTYPE),so)
EXTRA_LDFLAGS += -nodefaultlibs -nostartfiles -nostdlib
endif

##
## Assembler flags:
##
## -g                        Build with debugging symbol information.
##
## -I ...                    Specify include directories to search.
##

EXTRA_ASFLAGS += -Wa,-g

##
## For build executables, override the names even if set on the command line.
##

ifneq (, $(BUILD))
override CC = $(CC_FOR_BUILD)
override AR = $(AR_FOR_BUILD)
override OBJCOPY = $(OBJCOPY_FOR_BUILD)
override STRIP = $(STRIP_FOR_BUILD)
override CFLAGS = -Wall -Werror -O1
override BFD_ARCH = $(BUILD_BFD_ARCH)
override OBJ_FORMAT = $(BUILD_OBJ_FORMAT)
ifneq ($(DEBUG), chk)
override CFLAGS += -Wno-unused-but-set-variable
endif

override EXTRA_CFLAGS := $(EXTRA_CFLAGS_FOR_BUILD)
override EXTRA_CPPFLAGS := $(EXTRA_CPPFLAGS_FOR_BUILD)
override CPPFLAGS :=
endif

##
## Makefile targets. .PHONY specifies that the following targets don't actually
## have files associated with them.
##

.PHONY: test all clean wipe $(DIRS) $(TESTDIRS) prebuild postbuild

##
## prepend the current object directory to every extra directory.
##

EXTRA_OBJ_DIRS += $(EXTRA_SRC_DIRS:%=$(OBJROOT)/$(THISDIR)/%) $(STRIPPED_DIR)

all: $(DIRS) $(BINARY) postbuild

$(DIRS): $(OBJROOT)/$(THISDIR)
postbuild: $(BINARY)

test: all $(TESTDIRS)

$(DIRS):
	@$(CECHO_CYAN) Entering Directory: $(SRCROOT)/$(THISDIR)/$@ && \
	[ -d $@ ] || mkdir -p $@ && \
	$(MAKE) --no-print-directory -C $@ -f $(SRCDIR)/$@/Makefile \
	    $(MAKECMDGOALS) SRCDIR=$(SRCDIR)/$@ && \
	$(CECHO_CYAN) Leaving Directory: $(SRCROOT)/$(THISDIR)/$@

$(TESTDIRS): $(BINARY)
	@$(CECHO_CYAN) Entering Test Directory: $(SRCROOT)/$(THISDIR)/$@ && \
	[ -d $@ ] || mkdir -p $@ && \
	$(MAKE) --no-print-directory -C $@ -f $(SRCDIR)/$@/Makefile \
	    -I$(SRCDIR)/$@ $(MAKECMDGOALS) SRCDIR=$(SRCDIR)/$@ && \
	$(CECHO_CYAN) Leaving Test Directory: $(SRCROOT)/$(THISDIR)/$@

##
## The dependencies of the binary object depend on the architecture and type of
## binary being built.
##

ifneq (, $(BUILD))
SAVED_ARCH := $(ARCH)
ARCH := $(BUILD_ARCH)
endif

ifeq (x86, $(ARCH))
ALLOBJS = $(X86_OBJS) $(OBJS)
endif

ifeq (x64, $(ARCH))
ALLOBJS = $(X64_OBJS) $(OBJS)
endif

ifeq (armv7, $(ARCH))
ALLOBJS = $(ARMV7_OBJS) $(OBJS)
endif

ifeq (armv6, $(ARCH))
ALLOBJS = $(ARMV6_OBJS) $(OBJS)
endif

ifneq (, $(BUILD))
ARCH := $(SAVED_ARCH)
endif

ifneq (, $(strip $(ALLOBJS)))

##
## The object files are dependent on the object directory, but the object
## directory being newer should not trigger a rebuild of the object files.
##

$(ALLOBJS): | $(OBJROOT)/$(THISDIR)

$(BINARY): $(ALLOBJS) $(TARGETLIBS)
    ifeq ($(BINARYTYPE),app)
	@echo Linking - $@
	@$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) -pie -o $@ $^ -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),staticapp)
	@echo Linking - $@
	@$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) -static -o $@ -Wl,--start-group $^ -Wl,--end-group -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),ntconsole)
	@echo Linking - $@
	@$(CC) -o $@ $^ $(TARGETLIBS) -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),win32)
	@echo Linking - $@
	@$(CC) -mwindows -o $@ $^ $(TARGETLIBS) -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),dll)
	@echo Linking - $@
	@$(CC) -shared -o $@ $^ $(TARGETLIBS) -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),library)
	@echo Building Library - $@
	@$(AR) rcs $@ $^ $(TARGETLIBS)
    endif
    ifeq ($(BINARYTYPE),so)
	@echo Linking - $@
	@$(CC) $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) -shared -Wl,-soname=$(BINARY) -o $@ $^ -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINARYTYPE),build)
	@echo Linking - $@
	@$(CC) $(LDFLAGS) -o $@ $^ $(TARGETLIBS) -Bdynamic $(DYNLIBS)
    endif
    ifeq ($(BINPLACE),bin)
	@echo Binplacing - $(OBJROOT)/$(THISDIR)/$(BINARY)
	@cp -pf $(BINARY) $(BINROOT)/
	@$(STRIP) -p -o $(STRIPPED_DIR)/$(BINARY) $(BINARY)
	@$(UPDATE_LAST_UPDATE)
    endif
    ifeq ($(BINPLACE),testbin)
	@echo Binplacing Test - $(OBJROOT)/$(THISDIR)/$(BINARY)
	@cp -pf $(BINARY) $(TESTBIN)/
    endif

else

.PHONY: $(BINARY)

endif

##
## Prebuild is an "order-only" dependency of this directory, meaning that
## prebuild getting rebuilt does not cause this directory to need to be
## rebuilt.
##

$(OBJROOT)/$(THISDIR): | prebuild $(BINROOT) $(TESTBIN) $(EXTRA_OBJ_DIRS)
	@mkdir -p $(OBJROOT)/$(THISDIR)

$(BINROOT) $(TESTBIN) $(EXTRA_OBJ_DIRS):
	@mkdir -p $@

##
## Generic target specifying how to compile a file.
##

%.o:%.c
	@echo Compiling - $(notdir $<)
	@$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

##
## Generic target specifying how to assemble a file.
##

%.o:%.S
	@echo Assembling - $(notdir $<)
	@$(CC) $(CPPFLAGS) $(EXTRA_CPPFLAGS) $(ASFLAGS) $(EXTRA_ASFLAGS) -c -o $@ $<

##
## Generic target specifying how to compile a resource.
##

%.rsc:%.rc
	@echo Compiling Resource - $(notdir $<)
	@$(RCC) -o $@ $<

##
## This ends the originated-in-source-directory make.
##

endif

