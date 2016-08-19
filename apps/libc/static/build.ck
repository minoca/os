/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Static C Library

Abstract:

    This module contains the portion of the C library that is statically
    linked into every application. It contains little other than some
    initialization stubs.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode C Library

--*/

function build() {
    sources = [
        "init.c",
        "atexit.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/aatexit.c",
            "armv7/crt0.S"
        ];

    } else if (arch == "x86") {
        arch_sources = [
            "x86/crt0.S"
        ];

    } else if (arch == "x64") {
        arch_sources = [
            "x64/crt0.S"
        ];
    }

    includes = [
        "$//apps/libc/include"
    ];

    lib = {
        "label": "libc_nonshared",
        "inputs": arch_sources + sources,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
