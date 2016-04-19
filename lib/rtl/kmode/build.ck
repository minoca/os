/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Kernel Rtl

Abstract:

    This library contains kernel-specific functions for the Runtime Library.

Author:

    Evan Green 26-Jul-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "assert.c",
        "kprint.c"
    ];

    includes = [
        "$//lib/rtl"
    ];

    lib = {
        "label": "krtl",
        "inputs": sources,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
