/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    RTL C

Abstract:

    This library contains stub functions to allow the Rtl Library to
    run on top of a standard C library.

Author:

    Evan Green 23-Oct-2012

Environment:

    Any

--*/

function build() {
    sources = [
        "stubs.c"
    ];

    includes = [
        "$//lib/rtl"
    ];

    lib = {
        "label": "rtlc",
        "inputs": sources,
        "includes": includes,
    };

    build_lib = {
        "label": "build_rtlc",
        "output": "rtlc",
        "inputs": sources,
        "includes": includes,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();

