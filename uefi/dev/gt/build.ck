/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    ARM Generic Timer

Abstract:

    This library contains the ARM Generic Timer UEFI device.

Author:

    Chris Stevens 9-Jun-2016

Environment:

    Firmware

--*/

function build() {
    sources = [
        "gt.c",
        "gta.S",
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "gt",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
