/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Architecture Support

Abstract:

    This module contains architecure-specific UEFI core support functions.

Author:

    Evan Green 27-Mar-2014

Environment:

    Firmware

--*/

function build() {
    x86_sources = [
        "x86/archlib.c",
        "x86/archsup.S",
        "x86/ioport.S",
        "x86/regacces.c"
    ];

    arm_sources = [
        "armv7/archlib.c",
        "armv7/regacces.c"
    ];

    armv7_sources = arm_sources + [
        "armv7/archsup.S"
    ];

    armv6_sources = arm_sources + [
        "armv6/archsup.S"
    ];

    if (arch == "armv7") {
        sources = armv7_sources;

    } else if (arch == "armv6") {
        sources = armv6_sources;

    } else if (arch == "x86") {
        sources = x86_sources;

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
        "label": "uefiarch",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
