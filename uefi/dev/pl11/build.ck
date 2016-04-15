/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    PL-011 UART

Abstract:

    This library contains the ARM PrimeCell PL-011 UART controller library.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "pl11.c"
    ];

    includes = [
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "pl11",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
