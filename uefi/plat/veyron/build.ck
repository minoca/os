/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Veyron UEFI Firmware

Abstract:

    This module implements UEFI firmware for the ASUS C201 (Veyron Speedy) on
    the RK3288 SoC.

Author:

    Chris Stevens 6-Jul-2015

Environment:

    Firmware

--*/

function build() {
    plat = "veyron";
    text_address = "0x020000A4";
    sources = [
        "armv7/entry.S",
        "armv7/smpa.S",
        "debug.c",
        "fwvol.c",
        "intr.c",
        "main.c",
        "memmap.c",
        "ramdenum.c",
        "sd.c",
        "serial.c",
        "smbios.c",
        "smp.c",
        "timer.c",
        "usb.c",
        ":" + plat + "fwv.o",
        "video.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"]
    };

    //
    // Don't bother to page align data since the text segment is loaded at a
    // specific and unaligned place (hence the nmagic).
    //

    link_ldflags = [
        "-nostdlib",
        "-Wl,--no-wchar-size-warning",
        "-static",
        "-Wl,--nmagic"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

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
        "//uefi/core:emptyrd",
    ];

    libs = [
        "//uefi/dev/gic:gic",
        "//uefi/dev/ns16550:ns16550",
        "//uefi/dev/sd/dwc:sddwc",
        "//uefi/dev/sd/core:sd"
    ];

    libs += common_libs;
    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "config": link_config
    };

    entries = executable(elf);

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
    // Flatten the firmware image, convert to U-boot, and run the firmware
    // build process on it.
    //

    flattened = {
        "label": platfw + ".bin",
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    uboot_config = {
        "TEXT_ADDRESS": text_address,
        "MKUBOOT_FLAGS": "-c -a arm -f fit"
    };

    uboot_name = platfw + ".ubo";
    uboot = {
        "label": uboot_name,
        "inputs": [":" + platfw + ".bin"],
        "orderonly": ["//uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": uboot_config
    };

    entries += [uboot];
    fwb_sources = [
        "veyron.kbk",
        "veyron.pem",
        ":" + uboot_name
    ];

    fwb_config = {
        "TEXT_ADDRESS": text_address
    };

    fwb = {
        "type": "target",
        "label": platfw,
        "inputs": fwb_sources,
        "implicit": ["//uefi/plat/veyron/fwbuild:veyrnfwb"],
        "config": fwb_config,
        "tool": "veyrnfwb",
        "nostrip": TRUE
    };

    entries += binplace(fwb);
    return entries;
}

return build();
