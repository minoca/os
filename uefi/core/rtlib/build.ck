/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    UEFI Runtime Library

Abstract:

    This file is responsible for building the core UEFI Runtime support,
    which is compiled into most platform runtime images.

Author:

    Evan Green 18-Mar-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "//uefi/core:crc32.o",
        "//uefi/core:div.o",
        "init.c",
        "time.c",
        "//uefi/core:util.o",
        "var.c"
    ];

    includes = [
        "-I$///uefi/include",
        "-I$///uefi/core"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "rtlib",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
