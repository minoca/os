/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Raspberry Pi UEFI Firmware

Abstract:

    This module implements UEFI firmware for the Raspberry Pi.

Author:

    Chris Stevens 31-Dec-2014

Environment:

    Firmware

--*/

from menv import staticApplication, uefiFwvol, flattenedBinary;

function build() {
    var commonLibs;
    var elf;
    var entries;
    var ffs;
    var flattened;
    var fwVolume;
    var includes;
    var libs;
    var linkConfig;
    var plat = "rpi";
    var platfw;
    var sources;
    var sourcesConfig;
    var textAddress = "0x00008000";

    sources = [
        "armv6/entry.S",
        "debug.c",
        "fwvol.c",
        "intr.c",
        "main.c",
        "memmap.c",
        "ramdenum.c",
        ":" + plat + "fwv.o",
        "smbios.c",
        "timer.c",
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
        "uefi/core:emptyrd",
    ];

    libs = [
        "uefi/dev/pl11:pl11",
        "uefi/dev/bcm2709:bcm2709",
        "uefi/dev/sd/core:sd",
    ];

    libs += commonLibs;
    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": linkConfig,
        "text_address": textAddress
    };

    entries = staticApplication(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "uefi/core/runtime:rtbase.ffs",
        "uefi/plat/" + plat + "/runtime:" + plat + "rt.ffs",
        "uefi/plat/" + plat + "/acpi:acpi.ffs"
    ];

    fwVolume = uefiFwvol("uefi/plat/rpi", plat, ffs);
    entries += fwVolume;

    //
    // Flatten the firmware image.
    //

    flattened = {
        "label": platfw,
        "inputs": [":" + platfw + ".elf"],
        "implicit": ["uefi/plat/rpi2/blobs:blobs"],
        "binplace": "bin"
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    return entries;
}

