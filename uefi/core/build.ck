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
        "-I$///uefi/include",
        "-I$///uefi/core"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "ueficore",
        "inputs": sources,
        "sources_config": sources_config
    };

    emptyrd_lib = {
        "label": "emptyrd",
        "inputs": ["emptyrd.c"]
    };

    entries = static_library(lib);
    entries += static_library(emptyrd_lib);
    return entries;
}

return build();
