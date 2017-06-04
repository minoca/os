/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import binplace, staticApplication, uefiFwvol, flattenedBinary;

function build() {
    var commonLibs;
    var elf;
    var entries;
    var flattened;
    var ffs;
    var fwb;
    var fwbConfig;
    var fwbSources;
    var fwVolume;
    var includes;
    var libs;
    var linkConfig;
    var plat = "veyron";
    var platfw;
    var sources;
    var sourcesConfig;
    var textAddress = "0x020000A4";
    var ubootConfig;
    var ubootName;
    var uboot;

    sources = [
        "armv7/entry.S",
        "armv7/minttbl.S",
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
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"]
    };

    //
    // Don't bother to page align data since the text segment is loaded at a
    // specific and unaligned place (hence the nmagic).
    //

    linkConfig = {
        "LDFLAGS": ["-Wl,--no-wchar-size-warning", "-Wl,--nmagic"]
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
        "uefi/dev/gic:gic",
        "uefi/dev/ns16550:ns16550",
        "uefi/dev/sd/dwc:sddwc",
        "uefi/dev/sd/core:sd"
    ];

    libs += commonLibs;
    platfw = plat + "fw";
    elf = {
        "label": platfw + ".elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": linkConfig
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

    fwVolume = uefiFwvol("uefi/plat/veyron", plat, ffs);
    entries += fwVolume;

    //
    // Flatten the firmware image, convert to U-boot, and run the firmware
    // build process on it.
    //

    flattened = {
        "label": platfw + ".bin",
        "inputs": [":" + platfw + ".elf"]
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    ubootConfig = {
        "text_address": textAddress,
        "MKUBOOT_FLAGS": "-c -a arm -f fit"
    };

    ubootName = platfw + ".ubo";
    uboot = {
        "type": "target",
        "label": ubootName,
        "inputs": [":" + platfw + ".bin"],
        "orderonly": ["uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": ubootConfig
    };

    entries += [uboot];
    fwbSources = [
        "veyron.kbk",
        "veyron.pem",
        ":" + ubootName
    ];

    fwbConfig = {
        "text_address": textAddress
    };

    fwb = {
        "type": "target",
        "label": platfw,
        "inputs": fwbSources,
        "implicit": ["uefi/plat/veyron/fwbuild:veyrnfwb"],
        "config": fwbConfig,
        "tool": "veyrnfwb",
        "nostrip": true
    };

    entries += binplace(fwb);
    return entries;
}

