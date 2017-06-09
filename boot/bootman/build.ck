/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Boot Manager

Abstract:

    This module implements the boot manager, which can load an operating
    system loader. In a multi-boot system, there would be one boot manager
    that can load one of many OS loaders (including downstream loaders).

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

from menv import binplace, staticApplication, flattenedBinary, mconfig;

function build() {
    var arch = mconfig.arch;
    var baseRtl = "lib/rtl/base:basertl";
    var bootmanPe;
    var commonSources;
    var efiApp;
    var efiConfig;
    var efiLibs;
    var efiSources;
    var elfconvConfig;
    var entries;
    var flattened;
    var includes;
    var linkerScript;
    var pcatApp;
    var pcatConfig = {};
    var pcatLibs;
    var pcatSources;
    var sourcesConfig;
    var x6432 = "";

    commonSources = [
        "bootman.c",
        "bootim.c"
    ];

    pcatSources = [
        "pcat/x86/entry.S",
        ":bootman.o",
        ":bootim.o",
        "pcat/bootxfr.c",
        "pcat/main.c",
        "pcat/paging.c"
    ];

    efiSources = [
        "efi/bootxfr.c",
        "efi/main.c"
    ];

    includes = [
        "$S/boot/lib/include",
        "$S/boot/bootman"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    efiConfig = {
        "LDFLAGS": ["-pie"]
    };

    efiLibs = [
        "kernel/kd:kdboot",
        "kernel/hl:hlboot",
        "lib/im:imu",
        "lib/bconflib:bconf",
        "kernel/kd/kdusb:kdnousb",
        "boot/lib:bootefi",
        "lib/basevid:basevid",
        "lib/fatlib:fat",
        "kernel/mm:mmboot"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        linkerScript = "$S/uefi/include/link_arm.x";
        efiConfig["LDFLAGS"] += ["-Wl,--no-wchar-size-warning"];
        baseRtl = "lib/rtl/base:basertlb";
        efiLibs += ["kernel:archboot"];

    } else if (arch == "x86") {
        linkerScript = "$S/uefi/include/link_x86.x";
        pcatSources += [
            "pcat/x86/xferc.c"
        ];

    } else if (arch == "x64") {
        linkerScript = "$S/uefi/include/link_x64.x";
        pcatSources += [
            "pcat/x64/xfera.S",
            "pcat/x64/xferc.c"
        ];
    }

    efiLibs += [
        baseRtl,
        "lib/rtl/kmode:krtl"
    ];

    efiApp = {
        "label": "bootmefi.elf",
        "inputs": commonSources + efiSources + efiLibs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": efiConfig,
        "entry": "BmEfiApplicationMain",
        "linker_script": linkerScript
    };

    entries = staticApplication(efiApp);

    //
    // Convert the ELF image into an EFI PE image.
    //

    elfconvConfig = {
        "ELFCONV_FLAGS": "-t efiapp"
    };

    bootmanPe = {
        "type": "target",
        "label": "bootmefi.efi",
        "inputs": [":bootmefi.elf"],
        "implicit": ["uefi/tools/elfconv:elfconv"],
        "tool": "elfconv",
        "config": elfconvConfig,
        "nostrip": true
    };

    entries += binplace(bootmanPe);

    //
    // On PC machines, build the BIOS version as well. The boot manager is
    // 32-bits even on 64-bit machines so that both 32 and 64-bit OS loaders
    // can be launched. This means that on x64 all the libraries need to be
    // recompiled as 32-bit libraries.
    //

    if ((arch == "x86") || (arch == "x64")) {
        if (arch == "x64") {
            x6432 = "32";
            pcatConfig["LDFLAGS"] = ["-m32"];
            sourcesConfig["CPPFLAGS"] = ["-m32"];
        }

        pcatLibs = [
            "kernel/kd:kdboot" + x6432,
            "kernel/hl:hlboot" + x6432,
            "lib/im:imu" + x6432,
            "lib/bconflib:bconf" + x6432,
            "kernel/kd/kdusb:kdnousb" + x6432,
            "boot/lib:bootpcat" + x6432,
            "lib/partlib:partlib" + x6432,
            "lib/basevid:basevid" + x6432,
            "lib/fatlib:fat" + x6432,
            "kernel/mm:mmboot" + x6432,
            "lib/rtl/base:basertl" + x6432,
            "lib/rtl/kmode:krtl" + x6432
        ];

        pcatApp = {
            "label": "bootman",
            "inputs": pcatSources + pcatLibs,
            "sources_config": sourcesConfig,
            "includes": includes,
            "config": pcatConfig,
            "text_address": "0x100000",
            "binplace": "bin"
        };

        entries += staticApplication(pcatApp);

        //
        // Flatten the image so the VBR can load it directly into memory.
        //

        flattened = {
            "label": "bootman.bin",
            "inputs": [":bootman"],
            "binplace": "bin",
            "nostrip": true
        };

        flattened = flattenedBinary(flattened);
        entries += flattened;
    }

    return entries;
}

