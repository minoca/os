/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Integrator/CP UEFI Firmware

Abstract:

    This module implements UEFI firmware for the ARM Integrator/CP.

Author:

    Evan Green 4-Apr-2014

Environment:

    Firmware

--*/

from menv import addConfig, staticApplication, mconfig, uefiFwvol;

function build() {
    var commonLibs;
    var elf;
    var entries;
    var ffs;
    var fwVolume;
    var includes;
    var libs;
    var linkConfig;
    var msetupFlags;
    var plat = "integ";
    var ramdiskImage;
    var sources;
    var sourcesConfig;
    var textAddress = "0x80100000";

    sources = [
        "armv7/entry.S",
        "debug.c",
        "fwvol.c",
        ":" + plat + "fwv.o",
        "intr.c",
        "main.c",
        "memmap.c",
        "ramdenum.c",
        "ramdisk.S",
        "serial.c",
        "smbios.c",
        "timer.c",
        "video.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"]
    };

    linkConfig = {
        "LDFLAGS": ["-Wl,--no-wchar-size-warning"]
    };

    commonLibs = [
        "uefi/core:ueficore",
        "kernel/kd:kdboot",
        "uefi/core:ueficore",
        "uefi/archlib:uefiarch",
        "lib/fatlib:fat",
        "lib/basevid:basevid",
        "lib/rtl/base:basertlb",
        "kernel/kd/kdusb:kdnousb",
        "kernel:archboot",
    ];

    libs = commonLibs + [
        "uefi/dev/pl11:pl11",
        "uefi/dev/pl110:pl110"
    ];

    elf = {
        "label": plat + ".img",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": linkConfig,
        "text_address": textAddress,
        "binplace": "bin"
    };

    entries = staticApplication(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "uefi/core/runtime:rtbase.ffs",
        "uefi/plat/integcp/runtime:" + plat + "rt.ffs",
        "uefi/plat/integcp/acpi:acpi.ffs"
    ];

    fwVolume = uefiFwvol("uefi/plat/integcp", plat, ffs);
    entries += fwVolume;

    //
    // Create the RAM disk image. Debugging is always enabled in these images
    // since they're only used for development.
    //

    msetupFlags = [
        "-q",
        "-D",
        "-G30M",
        "-lintegrd",
        "-i$" + mconfig.binroot + "/install.img"
    ];

    ramdiskImage = {
        "type": "target",
        "tool": "msetup_image",
        "label": "ramdisk",
        "output": "ramdisk",
        "inputs": ["images:install.img"],
        "config": {"MSETUP_FLAGS": msetupFlags}
    };

    entries += [ramdiskImage];

    //
    // The ramdisk.o object depends on the ramdisk target just created.
    //

    for (entry in entries) {
        if ((entry.get("output")) && (entry.output.endsWith("ramdisk.o"))) {
            entry["implicit"] = [":ramdisk"];
            addConfig(entry, "CPPFLAGS", "-I$O/uefi/plat/integcp");
            break;
        }
    }

    return entries;
}

