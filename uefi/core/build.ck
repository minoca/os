/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    UEFI Core

Abstract:

    This file is responsible for building the core UEFI support. This is a
    library containing the bulk of the UEFI interfaces, which platform
    specific images include into their boot image.

Author:

    Evan Green 26-Feb-2014

Environment:

    Firmware

--*/

function build() {
    base_sources = [
        "acpi.c",
        "acpitabs.c",
        "basepe.c",
        "bdsboot.c",
        "bdscon.c",
        "bdsentry.c",
        "bdsutil.c",
        "cfgtable.c",
        "crc32.c",
        "dbgser.c",
        "devpathu.c",
        "diskio.c",
        "dispatch.c",
        "div.c",
        "drvsup.c",
        "event.c",
        "fatdev.c",
        "fatfs.c",
        "fsvars.c",
        "fvblock.c",
        "fvsect.c",
        "fwvol.c",
        "fwvolio.c",
        "handle.c",
        "image.c",
        "init.c",
        "intr.c",
        "locate.c",
        "lock.c",
        "memory.c",
        "part.c",
        "partelto.c",
        "partgpt.c",
        "partmbr.c",
        "pool.c",
        "ramdisk.c",
        "smbios.c",
        "stubs.c",
        "tpl.c",
        "timer.c",
        "util.c",
        "version.c",
        "vidcon.c",
    ];

    x86_sources = [
        "x86/archsup.S",
        "x86/prochw.c"
    ];

    arm_sources = [
        "armv7/commsup.S",
        "armv7/inttable.S",
        "armv7/prochw.c"
    ];

    if ((arch == "armv6") || (arch == "armv7")) {
        sources = base_sources + arm_sources;

    } else if (arch == "x86") {
        sources = base_sources + x86_sources;

    } else {

        assert(0, "Unknown architecture");
    }

    includes = [
        "$//uefi/include",
        "$//uefi/core"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    //
    // Create the version.h header.
    //

    fw_core_version_major = "1";
    fw_core_version_minor = "0";
    fw_core_version_revision = "0";
    entries = create_version_header(fw_core_version_major,
                                    fw_core_version_minor,
                                    fw_core_version_revision);

    lib = {
        "label": "ueficore",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    emptyrd_lib = {
        "label": "emptyrd",
        "inputs": ["emptyrd.c"]
    };

    entries += static_library(lib);

    //
    // Add the include and dependency for version.c.
    //

    for (entry in entries) {
        if (entry["output"] == "version.o") {
            add_config(entry, "CPPFLAGS", "-I$^/uefi/core");
            entry["implicit"] = [":version.h"];
            break;
        }
    }

    entries += static_library(emptyrd_lib);
    return entries;
}

return build();
