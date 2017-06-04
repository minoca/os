/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PandaBoard UEFI Firmware

Abstract:

    This module implements UEFI firmware for the TI PandaBoard.

Author:

    Evan Green 19-Dec-2014

Environment:

    Firmware

--*/

from menv import addConfig, binplace, staticApplication, uefiFwvol,
    flattenedBinary, mconfig, kernelLibrary;

function build() {
    var commonLibs;
    var elf;
    var entries;
    var entry;
    var flattened;
    var flattenedUsb;
    var ffs;
    var fwLib;
    var fwVolume;
    var includes;
    var libfw;
    var libfwTarget;
    var libs;
    var linkConfig;
    var msetupFlags;
    var plat = "panda";
    var platfw;
    var platfwUsb;
    var sources;
    var sourcesConfig;
    var textAddress = "0x82000000";
    var uboot;
    var ubootConfig;
    var ubootUsb;
    var usbElf;

    sources = [
        "armv7/entry.S",
        "armv7/smpa.S",
        "debug.c",
        "fwvol.c",
        "uefi/plat/panda/init:id.o",
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

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"]
    };

    linkConfig = {
        "LDFLAGS": ["-Wl,--no-wchar-size-warning"]
    };

    platfw = plat + "fw";
    platfwUsb = platfw + "_usb";
    libfw = "lib" + platfw;
    libfwTarget = ":" + libfw;
    commonLibs = [
        "uefi/core:ueficore",
        "kernel/kd:kdboot",
        "uefi/core:ueficore",
        "uefi/archlib:uefiarch",
        "lib/fatlib:fat",
        "lib/basevid:basevid",
        "lib/rtl/kmode:krtl",
        "lib/rtl/base:basertlb",
        "kernel/kd/kdusb:kdnousb",
        "kernel:archboot",
    ];

    libs = [
        libfwTarget,
        "uefi/dev/gic:gic",
        "uefi/dev/sd/core:sd",
        "uefi/dev/omap4:omap4",
        "uefi/dev/omapuart:omapuart",
    ];

    libs += commonLibs + [libfwTarget];
    fwLib = {
        "label": libfw,
        "inputs": sources,
        "sources_config": sourcesConfig
    };

    entries = kernelLibrary(fwLib);
    elf = {
        "label": platfw + ".elf",
        "inputs": libs + ["uefi/core:emptyrd"],
        "sources_config": sourcesConfig,
        "includes": includes,
        "text_address": textAddress,
        "config": linkConfig
    };

    entries += staticApplication(elf);
    usbElf = {
        "label": platfwUsb + ".elf",
        "inputs": ["ramdisk.S"] + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "text_address": textAddress,
        "config": linkConfig
    };

    entries += staticApplication(usbElf);

    //
    // Build the firmware volume.
    //

    ffs = [
        "uefi/core/runtime:rtbase.ffs",
        "uefi/plat/" + plat + "/runtime:" + plat + "rt.ffs",
        "uefi/plat/" + plat + "/acpi:acpi.ffs"
    ];

    fwVolume = uefiFwvol("uefi/plat/panda", plat, ffs);
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
    flattenedUsb = {
        "label": platfwUsb + ".bin",
        "inputs": [":" + platfwUsb + ".elf"]
    };

    flattenedUsb = flattenedBinary(flattenedUsb);
    entries += flattenedUsb;
    ubootConfig = {
        "text_address": textAddress,
        "MKUBOOT_FLAGS": "-c -a arm -f legacy"
    };

    uboot = {
        "label": platfw,
        "inputs": [":" + platfw + ".bin"],
        "orderonly": ["uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": ubootConfig,
        "nostrip": true
    };

    entries += binplace(uboot);
    ubootUsb = {
        "label": "pandausb.img",
        "inputs": [":" + platfwUsb + ".bin"],
        "orderonly": ["uefi/tools/mkuboot:mkuboot"],
        "tool": "mkuboot",
        "config": ubootConfig,
        "nostrip": true
    };

    entries += binplace(ubootUsb);

    //
    // Create the RAM disk image. Debugging is always enabled in these RAM disk
    // images since they're only used for development.
    //

    msetupFlags = [
        "-q",
        "-G30M",
        "-lpanda-usb",
        "-i" + mconfig.binroot + "/install.img",
        "-D"
    ];

    entry = {
        "type": "target",
        "tool": "msetup_image",
        "label": "ramdisk",
        "output": "ramdisk",
        "inputs": ["images:install.img"],
        "config": {"MSETUP_FLAGS": msetupFlags}
    };

    entries += [entry];

    //
    // The ramdisk.o object depends on the ramdisk target just created.
    //

    for (entry in entries) {
        if ((entry.get("output")) && (entry.output.endsWith("ramdisk.o"))) {
            entry["implicit"] = [":ramdisk"];
            addConfig(entry, "CPPFLAGS", "-I$O/uefi/plat/panda");
            break;
        }
    }

    return entries;
}

