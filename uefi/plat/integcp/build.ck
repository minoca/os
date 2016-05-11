/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Integrator/CP UEFI Firmware

Abstract:

    This module implements UEFI firmware for the ARM Integrator/CP.

Author:

    Evan Green 4-Apr-2014

Environment:

    Firmware

--*/

function build() {
    plat = "integ";
    text_address = "0x80100000";
    sources = [
        "armv7/entry.S",
        "debug.c",
        "fwvol.c",
        ":" + plat + "fwv.o",
        "intr.c",
        "main.c",
        "memmap.c",
        "ramdenum.c",
        ":ramdisk.o",
        "serial.c",
        "smbios.c",
        "timer.c",
        "video.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"]
    };

    link_ldflags = [
        "-nostdlib",
        "-Wl,--no-wchar-size-warning",
        "-static"
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
        "//lib/rtl/base:basertlb",
        "//kernel/kd/kdusb:kdnousb",
        "//kernel:archboot",
    ];

    libs = common_libs + [
        "//uefi/dev/pl11:pl11",
        "//uefi/dev/pl110:pl110"
    ];

    elf = {
        "label": plat + ".img",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "config": link_config,
        "text_address": text_address,
        "binplace": TRUE
    };

    entries = executable(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "//uefi/core/runtime:rtbase.ffs",
        "//uefi/plat/integcp/runtime:" + plat + "rt.ffs",
        "//uefi/plat/integcp/acpi:acpi.ffs"
    ];

    fw_volume = uefi_fwvol_o(plat, ffs);
    entries += fw_volume;

    //
    // Create the RAM disk image. Debugging is always enabled in these images
    // since they're only used for development.
    //

    msetup_flags = [
        "-q",
        "-D",
        "-G30M",
        "-lintegrd",
        "-i$" + binroot + "/install.img"
    ];

    ramdisk_image = {
        "type": "target",
        "tool": "msetup_image",
        "label": "ramdisk",
        "output": "ramdisk",
        "inputs": ["//images:install.img"],
        "config": {"MSETUP_FLAGS": msetup_flags}
    };

    entries += [ramdisk_image];
    ramdisk_o = {
        "type": "target",
        "label": "ramdisk.o",
        "inputs": [":ramdisk"],
        "tool": "objcopy"
    };

    entries += [ramdisk_o];
    return entries;
}

return build();
