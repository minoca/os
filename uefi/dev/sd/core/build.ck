/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    SD Core

Abstract:

    This library contains the generic Secure Digital (card) controller
    device.

Author:

    Evan Green 20-Mar-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "sd.c",
        "sdstd.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "sd",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
