/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    NS 16550 UART

Abstract:

    This library contains the NS 16550 UART controller library.

Author:

    Chris Stevens 10-Jul-2015

Environment:

    Firmware

--*/

function build() {
    sources = [
        "ns16550.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "ns16550",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
