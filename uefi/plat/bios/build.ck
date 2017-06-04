/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    var plat = "bios";
    var platfw;
    var sources;
    var sourcesConfig;
    var textAddress = "0x100000";

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

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"]
    };

    commonLibs = [
        "uefi/core:ueficore",
        "kernel/kd:kdboot",
        "uefi/core:ueficore",
        "uefi/archlib:uefiarch",
        "lib/fatlib:fat",
        "lib/basevid:basevid",
        "lib/rtl/base:basertl",
        "kernel/kd/kdusb:kdnousb",
        "uefi/core:emptyrd",
    ];

    libs = [
        "uefi/dev/ns16550:ns16550"
    ];

    libs += commonLibs;
    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
    };

    entries = staticApplication(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "uefi/core/runtime:rtbase.ffs",
        "uefi/plat/bios/runtime:" + plat + "rt.ffs"
    ];

    fwVolume = uefiFwvol("uefi/plat/bios", plat, ffs);
    entries += fwVolume;
    flattened = {
        "label": platfw,
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    return entries;
}

