/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    TI OMAP4

Abstract:

    This library contains the OMAP4 UEFI device support.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "clock.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "omap4",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
