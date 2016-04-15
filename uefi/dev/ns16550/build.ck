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
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "ns16550",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
