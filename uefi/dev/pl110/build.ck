/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    PL-110 Video

Abstract:

    This library contains the ARM PL-110 Video Controller device support.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "pl110.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "pl110",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
