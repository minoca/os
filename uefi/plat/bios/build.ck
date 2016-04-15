/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    BIOS UEFI Firmware

Abstract:

    This module implements a UEFI-compatible firmware layer on top of a
    legacy PC/AT BIOS.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

function build() {
    plat = "bios";
    text_address = "0x100000";
    sources = [
        "acpi.c",
        "bioscall.c",
        ":" + plat + "fwv.o",
        "disk.c",
        "debug.c",
        "fwvol.c",
        "intr.c",
        "main.c",
        "memmap.c",
        "timer.c",
        "video.c",
        "x86/entry.S",
        "x86/realmexe.S"
    ];

    fw_cppflags = [
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + fw_cppflags
    };

    link_ldflags = [
        "-nostdlib",
        "-static"
    ];

    link_config = {
        "LDFLAGS": ["$LDFLAGS"] + link_ldflags
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
        "//uefi/core:emptyrd",
    ];

    libs = [
        "//uefi/dev/ns16550:ns16550"
    ];

    libs += common_libs;
    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "config": link_config
    };

    entries = executable(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "//uefi/core/runtime:rtbase.ffs",
        "//uefi/plat/bios/runtime:" + plat + "rt.ffs"
    ];

    fw_volume = uefi_fwvol_o(plat, ffs);
    entries += fw_volume;
    flattened = {
        "label": platfw,
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattened_binary(flattened);
    entries += flattened;
    return entries;
}

return build();
