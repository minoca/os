/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    OMAP UART

Abstract:

    This library contains the UART controller library used in Texas
    Instruments OMAP3 and OMAP4 SoCs.

Author:

    Evan Green 27-Feb-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "omapuart.c"
    ];

    includes = [
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "omapuart",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
