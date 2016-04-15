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
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "omap4",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
