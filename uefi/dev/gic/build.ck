/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    ARM GIC Interrupt Controller

Abstract:

    This library contains the ARM Generic Interrupt Controller UEFI device.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "gic.c"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "gic",
        "inputs": sources,
        "sources_config": sources_config,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
