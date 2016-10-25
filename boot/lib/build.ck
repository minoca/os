/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Boot Library

Abstract:

    This library contains support routines for the boot applications.

Author:

    Evan Green 19-Feb-2014

Environment:

    Boot

--*/

function build() {
    common_sources = [
        "bootfat.c",
        "bootmem.c",
        "file.c",
        "stubs.c",
        "version.c"
    ];

    pcat_sources = [
        "pcat/fwapi.c",
        "pcat/int10.c",
        "pcat/int13.c",
        "pcat/memory.c",
        "pcat/realmexe.S",
        "pcat/realmode.c",
        "pcat/reset.c",
        "pcat/rsdp.c",
        "pcat/time.c"
    ];

    efi_sources = [
        "efi/dbgser.c",
        "efi/disk.c",
        "efi/fwapi.c",
        "efi/memory.c",
        "efi/time.c",
        "efi/util.c",
        "efi/video.c"
    ];

    common_arm_sources = [
        "armv7/commsup.S",
        "armv7/inttable.S",
        "armv7/prochw.c"
    ];

    common_arm_efi_sources = [
        "efi/armv7/efia.S",
        "efi/armv7/efiarch.c"
    ];

    if (arch == "armv7") {
        common_sources += common_arm_sources + [
            "armv7/archsup.S"
        ];

        efi_sources += common_arm_efi_sources;

    } else if (arch == "armv6") {
        common_sources += common_arm_sources + [
            "armv6/archsup.S"
        ];

        efi_sources += common_arm_efi_sources;

    } else if (arch == "x86") {
        common_sources += [
            "x86/archsup.S",
            "x86/prochw.c"
        ];

        efi_sources += [
            "efi/x86/efia.S",
            "efi/x86/efiarch.c"
        ];
    }

    includes = [
        "$//boot/lib/include",
        "$//boot/lib"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    common_lib = {
        "inputs": common_sources,
        "sources_config": sources_config,
        "includes": includes
    };

    common_list = compiled_sources(common_lib);
    entries = common_list[1];

    //
    // Add the include and dependency for version.c, which uses the kernel's
    // version.h
    //

    for (entry in entries) {
        if (entry["output"] == "version.o") {
            add_config(entry, "CPPFLAGS", "-I$^/kernel");
            entry["implicit"] = ["//kernel:version.h"];
            break;
        }
    }

    efi_lib = {
        "label": "bootefi",
        "inputs": common_list[0] + efi_sources,
        "sources_config": sources_config
    };

    entries += static_library(efi_lib);

    //
    // On PC machines, build the BIOS library as well.
    //

    if (arch == "x86") {
        pcat_lib = {
            "label": "bootpcat",
            "inputs": common_list[0] + pcat_sources,
            "sources_config": sources_config,
            "includes": includes
        };

        entries += static_library(pcat_lib);
    }

    return entries;
}

return build();
