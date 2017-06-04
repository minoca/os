/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    BeagleBone UEFI Firmware

Abstract:

    This module implements UEFI firmware for the TI BeagleBone Black.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

from menv import binplace, staticApplication, uefiFwvol, flattenedBinary;

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
    var plat = "bbone";
    var platfw;
    var sources;
    var sourcesConfig;
    var textAddress = "0x82000000";
    var uboot;
    var ubootConfig;

    sources = [
        "armv7/entry.S",
        "clock.c",
        "debug.c",
        "fwvol.c",
        "i2c.c",
        "intr.c",
        "main.c",
        "memmap.c",
        ":" + plat + "fwv.o",
        "ramdenum.c",
        "sd.c",
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
        "LDFLAGS": ["-Wl,--no-wchar-size-warning"],
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

    libs = commonLibs + [
        "uefi/dev/sd/core:sd",
        "uefi/dev/omapuart:omapuart"
    ];

    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "text_address": textAddress,
        "includes": includes,
        "config": linkConfig
    };

    entries = staticApplication(elf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "uefi/core/runtime:rtbase.ffs",
        "uefi/plat/beagbone/runtime:" + plat + "rt.ffs",
        "uefi/plat/beagbone/acpi:acpi.ffs"
    ];

    fwVolume = uefiFwvol("uefi/plat/beagbone", plat, ffs);
    entries += fwVolume;

    //
    // Flatten the firmware image and convert to u-boot.
    //

    flattened = {
        "label": platfw + ".bin",
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    ubootConfig = {
        "text_address": textAddress,
        "MKUBOOT_FLAGS": "-c -a arm -f legacy"
    };

    uboot = {
        "type": "target",
        "label": platfw,
        "inputs": [":" + platfw + ".bin"],
        "orderonly": ["uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": ubootConfig,
        "nostrip": true
    };

    entries += binplace(uboot);
    return entries;
}

