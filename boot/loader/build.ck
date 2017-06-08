/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    OS Loader

Abstract:

    This module implements the operating system loader that loads and
    launches the kernel. It is loaded by the Boot Manager, and is usually
    tied to a specific kernel version.

Author:

    Evan Green 29-Jul-2012

Environment:

    Boot

--*/

from menv import staticApplication, mconfig;

function build() {
    var arch = mconfig.arch;
    var baseLibs;
    var baseRtl = "lib/rtl/base:basertl";
    var commonArmSources;
    var commonLibs;
    var commonSources;
    var efiApp;
    var efiAppLibs;
    var efiLibs;
    var efiLinkConfig;
    var efiLinkLdflags;
    var efiSources;
    var entries;
    var includes;
    var linkLdflags;
    var pcatApp;
    var pcatAppLibs;
    var pcatLibs;
    var pcatLinkConfig;
    var pcatLinkLdflags;
    var pcatSources;
    var sourcesConfig;

    commonSources = [
        "bootim.c",
        "dbgport.c",
        "loader.c"
    ];

    pcatSources = [
        "pcat/memory.c",
    ];

    efiSources = [
        "efi/memory.c",
    ];

    includes = [
        "$S/boot/lib/include",
        "$S/boot/loader"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar", "$KERNEL_CFLAGS"],
    };

    linkLdflags = ["-pie"];
    efiLinkLdflags = ["-pie"];
    efiLibs = [
        "boot/lib:bootefi"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        commonArmSources = [
            "armv7/dbgparch.c",
            "armv7/paging.c",
            "armv7/kernxfr.S"
        ];

        if (arch == "armv7") {
            commonSources += commonArmSources + [
                "armv7/archsupc.c"
            ];

        } else {
            commonSources += commonArmSources + [
                "armv6/archsupc.c"
            ];
        }

        efiLinkLdflags += ["-Wl,--no-wchar-size-warning"];
        efiLibs += [
            "kernel:archboot",
        ];

        baseRtl = "lib/rtl/base:basertlb";

    } else if (arch == "x86") {
        commonSources += [
            "x86/archsupc.c",
            "x86/dbgparch.c",
            "x86/entry.S",
            "x86/paging.c",
            "x86/kernxfr.S"
        ];

    } else if (arch == "x64") {
        commonSources += [
            "x64/kernxfr.S",
            "x64/paging.c",
            "x86/archsupc.c",
            "x86/dbgparch.c",
            "x86/entry.S",
        ];
    }

    commonLibs = [
        "kernel/kd:kdboot",
        "kernel/hl:hlboot",
        "lib/im:imn",
        "lib/bconflib:bconf",
        "kernel/kd/kdusb:kdnousb"
    ];

    baseLibs = [
        "lib/basevid:basevid",
        "lib/fatlib:fat",
        "kernel/mm:mmboot",
        baseRtl,
        "lib/rtl/kmode:krtl",
    ];

    efiLinkConfig = {
        "LDFLAGS": efiLinkLdflags
    };

    pcatLinkConfig = {
        "LDFLAGS": linkLdflags
    };

    pcatLibs = [
        "boot/lib:bootpcat",
        "lib/partlib:partlib"
    ];

    efiAppLibs = commonLibs + efiLibs + baseLibs;
    efiApp = {
        "label": "loadefi",
        "inputs": commonSources + efiSources + efiAppLibs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "config": efiLinkConfig,
        "entry": "BoMain",
        "binplace": "bin"
    };

    entries = staticApplication(efiApp);

    //
    // On PC machines, build the BIOS loader as well.
    //

    if ((arch == "x86") || (arch == "x64")) {
        pcatAppLibs = commonLibs + pcatLibs + baseLibs;
        pcatApp = {
            "label": "loader",
            "inputs": commonSources + pcatSources + pcatAppLibs,
            "sources_config": sourcesConfig,
            "includes": includes,
            "config": pcatLinkConfig,
            "entry": "BoMain",
            "prefix": "pcat",
            "binplace": "bin"
        };

        entries += staticApplication(pcatApp);
    }

    return entries;
}

