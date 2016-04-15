/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    PandaBoard UEFI Firmware

Abstract:

    This module implements UEFI firmware for the TI PandaBoard.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

function build() {
    plat = "panda";
    text_address = "0x82000000";
    sources = [
        "armv7/entry.S",
        "armv7/smpa.S",
        "debug.c",
        "fwvol.c",
        "//uefi/plat/panda/init:id.o",
        "intr.c",
        "main.c",
        "memmap.c",
        "omap4usb.c",
        ":" + plat + "fwv.o",
        "ramdenum.c",
        "sd.c",
        "serial.c",
        "smbios.c",
        "smp.c",
        "timer.c",
        "video.c"
    ];

    //
    // TODO: Figure out the build date and revision.
    //

    fw_cppflags = [
        "-I$///uefi/include",
        "-DFIRMWARE_BUILD_DATE=\\\"08/15/2014\\\"",
        "-DFIRMWARE_VERSION_MAJOR=1",
        "-DFIRMWARE_VERSION_MINOR=0",
        "-DFIRMWARE_VERSION_STRING=\\\"1.0.0\\\""
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + fw_cppflags
    };

    link_ldflags = [
        "-nostdlib",
        "-Wl,--no-wchar-size-warning",
        "-static"
    ];

    link_config = {
        "LDFLAGS": ["$LDFLAGS"] + link_ldflags
    };

    platfw = plat + "fw";
    platfw_usb = platfw + "_usb";
    libfw = "lib" + platfw;
    libfw_target = ":" + libfw;
    common_libs = [
        "//uefi/core:ueficore",
        "//kernel/kd:kdboot",
        "//uefi/core:ueficore",
        "//uefi/archlib:uefiarch",
        "//lib/fatlib:fat",
        "//lib/basevid:basevid",
        "//lib/rtl/kmode:krtl",
        "//lib/rtl/base:basertlb",
        "//kernel/kd/kdusb:kdnousb",
        "//kernel:archboot",
    ];

    libs = [
        libfw_target,
        "//uefi/dev/gic:gic",
        "//uefi/dev/sd/core:sd",
        "//uefi/dev/omap4:omap4",
        "//uefi/dev/omapuart:omapuart",
    ];

    libs += common_libs + [libfw_target];
    fw_lib = {
        "label": libfw,
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(fw_lib);
    elf = {
        "label": platfw + ".elf",
        "inputs": libs + ["//uefi/core:emptyrd"],
        "sources_config": sources_config,
        "config": link_config
    };

    entries += executable(elf);
    usb_elf = {
        "label": platfw_usb + ".elf",
        "inputs": [":ramdisk.o"] + libs,
        "sources_config": sources_config,
        "config": link_config
    };

    entries += executable(usb_elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "//uefi/core/runtime:rtbase.ffs",
        "//uefi/plat/" + plat + "/runtime:" + plat + "rt.ffs",
        "//uefi/plat/" + plat + "/acpi:acpi.ffs"
    ];

    fw_volume = uefi_fwvol_o(plat, ffs);
    entries += fw_volume;

    //
    // Flatten the firmware image and convert to u-boot.
    //

    flattened = {
        "label": platfw + ".bin",
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    flattened_usb = {
        "label": platfw_usb + ".bin",
        "inputs": [":" + platfw_usb + ".elf"]
    };

    flattened_usb = flattened_binary(flattened_usb);
    entries += flattened_usb;
    uboot_config = {
        "TEXT_ADDRESS": text_address,
        "MKUBOOT_FLAGS": "-c -a arm -f legacy"
    };

    uboot = {
        "label": platfw,
        "inputs": [":" + platfw + ".bin"],
        "orderonly": ["//uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": uboot_config
    };

    uboot_usb = {
        "label": "pandausb.img",
        "inputs": [":" + platfw_usb + ".bin"],
        "orderonly": ["//uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": uboot_config
    };

    entries += [uboot, uboot_usb];

    //
    // Create the RAM disk for the USB version.
    // TODO: Change inputs to //images/pandard.img
    //

    ramdisk = copy(["//uefi/plat/beagbone:bbonefw"], "ramdisk", null, null);
    entries += ramdisk;
    ramdisk_o = {
        "type": "target",
        "label": "ramdisk.o",
        "inputs": [":ramdisk"],
        "tool": "objcopy",
        "config": {"OBJCOPY_FLAGS": global_config["OBJCOPY_FLAGS"]}
    };

    entries += [ramdisk_o];
    return entries;
}

return build();
