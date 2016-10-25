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

function build() {
    common_sources = [
        "bootim.c",
        "dbgport.c",
        "loader.c"
    ];

    pcat_sources = [
        "pcat/memory.c",
    ];

    efi_sources = [
        "efi/memory.c",
    ];

    includes = [
        "$//boot/lib/include",
        "$//boot/loader"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    link_ldflags = [
        "-nostdlib",
        "-pie",
        "-static"
    ];

    efi_libs = [
        "//boot/lib:bootefi"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        common_arm_sources = [
            "armv7/dbgparch.c",
            "armv7/paging.c",
            "armv7/kernxfr.S"
        ];

        if (arch == "armv7") {
            common_sources += common_arm_sources + [
                "armv7/archsupc.c"
            ];

        } else {
            common_sources += common_arm_sources + [
                "armv6/archsupc.c"
            ];
        }

        efi_link_ldflags = link_ldflags + [
            "-Wl,--no-wchar-size-warning"
        ];

        efi_libs = ["//kernel:archboot"] + efi_libs;

    } else if (arch == "x86") {
        efi_link_ldflags = link_ldflags;
        common_sources += [
            "x86/archsupc.c",
            "x86/dbgparch.c",
            "x86/entry.S",
            "x86/paging.c",
            "x86/kernxfr.S"
        ];
    }

    efi_link_config = {
        "LDFLAGS": efi_link_ldflags
    };

    pcat_link_config = {
        "LDFLAGS": link_ldflags
    };

    //
    // These base libraries are relied upon by the boot library and so they
    // must go after the boot library.
    //

    base_libs = [
        "//lib/basevid:basevid",
        "//lib/fatlib:fat",
        "//kernel/mm:mmboot",
        "//lib/rtl/kmode:krtl",
        "//lib/rtl/base:basertlb"
    ];

    common_libs = [
        "//kernel/kd:kdboot",
        "//kernel/hl:hlboot",
        "//lib/im:im",
        "//lib/bconflib:bconf",
        "//kernel/kd/kdusb:kdnousb"
    ];

    pcat_libs = [
        "//boot/lib:bootpcat",
        "//lib/partlib:partlib"
    ];

    efi_app_libs = common_libs + efi_libs + base_libs;
    efi_app = {
        "label": "loadefi",
        "inputs": common_sources + efi_sources + efi_app_libs,
        "sources_config": sources_config,
        "includes": includes,
        "config": efi_link_config,
        "entry": "BoMain",
        "binplace": TRUE
    };

    entries = application(efi_app);

    //
    // On PC machines, build the BIOS loader as well.
    //

    if (arch == "x86") {
        pcat_app_libs = common_libs + pcat_libs + base_libs;
        pcat_app = {
            "label": "loader",
            "inputs": common_sources + pcat_sources + pcat_app_libs,
            "sources_config": sources_config,
            "includes": includes,
            "config": pcat_link_config,
            "entry": "BoMain",
            "prefix": "pcat",
            "binplace": TRUE
        };

        entries += application(pcat_app);
    }

    return entries;
}

return build();
