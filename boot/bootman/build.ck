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

from menv import application, binplace, executable, flattenedBinary, mconfig;

function build() {
    var arch = mconfig.arch;
    var baseLibs;
    var bootmanPe;
    var commonLibs;
    var commonSources;
    var efiApp;
    var efiAppLibs;
    var efiLibs;
    var efiLinkConfig;
    var efiLinkLdflags;
    var efiSources;
    var elfconvConfig;
    var entries;
    var flattened;
    var includes;
    var linkerScript;
    var pcatApp;
    var pcatAppLibs;
    var pcatLibs;
    var pcatLinkConfig;
    var pcatLinkLdflags;
    var pcatSources;
    var sourcesConfig;

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

    efiLinkLdflags = [
        "-nostdlib",
        "-pie",
        "-static",
    ];

    pcatLinkLdflags = [
        "-nostdlib",
        "-static"
    ];

    efiLibs = [
        "boot/lib:bootefi",
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        linkerScript = "$S/uefi/include/link_arm.x";
        efiLinkLdflags += [
            "-Wl,--no-wchar-size-warning"
        ];

        efiLibs = ["kernel:archboot"] + efiLibs;

    } else if (arch == "x86") {
        linkerScript = "$S/uefi/include/link_x86.x";
    }

    efiLinkConfig = {
        "LDFLAGS": efiLinkLdflags
    };

    pcatLinkConfig = {
        "LDFLAGS": pcatLinkLdflags
    };

    //
    // These base libraries are relied upon by the boot library and so they
    // must go after the boot library.
    //

    baseLibs = [
        "lib/basevid:basevid",
        "lib/fatlib:fat",
        "kernel/mm:mmboot",
        "lib/rtl/kmode:krtl",
        "lib/rtl/base:basertlb"
    ];

    commonLibs = [
        "kernel/kd:kdboot",
        "kernel/hl:hlboot",
        "lib/im:imu",
        "lib/bconflib:bconf",
        "kernel/kd/kdusb:kdnousb"
    ];

    pcatLibs = [
        "boot/lib:bootpcat",
        "lib/partlib:partlib"
    ];

    efiAppLibs = commonLibs + efiLibs + baseLibs;
    efiApp = {
        "label": "bootmefi.elf",
        "inputs": commonSources + efiSources + efiAppLibs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": efiLinkConfig,
        "entry": "BmEfiApplicationMain",
        "linker_script": linkerScript
    };

    entries = application(efiApp);

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
    // On PC machines, build the BIOS library as well.
    //

    if (arch == "x86") {
        pcatAppLibs = commonLibs + pcatLibs + baseLibs;
        pcatApp = {
            "label": "bootman.elf",
            "inputs": pcatSources + pcatAppLibs,
            "sources_config": sourcesConfig,
            "includes": includes,
            "config": pcatLinkConfig,
            "text_address": "0x100000",
            "binplace": "bin"
        };

        entries += executable(pcatApp);

        //
        // Flatten the image so the VBR can load it directly into memory.
        //

        flattened = {
            "label": "bootman.bin",
            "inputs": [":bootman.elf"],
            "binplace": "bin",
            "nostrip": true
        };

        flattened = flattenedBinary(flattened);
        entries += flattened;
    }

    return entries;
}

