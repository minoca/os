/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    PL-031 RTC

Abstract:

    This library contains the ARM PrimeCell PL-031 Real Time Clock
    device support.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

function build() {
    sources = [
        "pl031.c"
    ];

    includes = [
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    lib = {
        "label": "pl031",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
