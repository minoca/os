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

    //
    // TODO: Figure out the build date and revision.
    //

    fw_cppflags = [
        "-I$///uefi/include",
        "-DFIRMWARE_BUILD_DATE=\\\"01/01/2016\\\"",
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

    libs = common_libs + [
        "//uefi/dev/pl11:pl11",
        "//uefi/dev/pl110:pl110"
    ];

    platfw = plat + "fw";
    elf = {
        "label": platfw,
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
        "//uefi/plat/integcp/runtime:" + plat + "rt.ffs",
        "//uefi/plat/integcp/acpi:acpi.ffs"
    ];

    fw_volume = uefi_fwvol_o(plat, ffs);
    entries += fw_volume;

    //
    // TODO: Change inputs to //images/integrd.img
    //

    ramdisk = copy("//uefi/plat/beagbone:bbonefw", "ramdisk", null, null, null);
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
