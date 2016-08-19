/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Static pthread

Abstract:

    This module implements functions that must be statically linked for the
    POSIX thread library.

Author:

    Evan Green 4-May-2015

Environment:

    User Mode C Library

--*/

function build() {
    sources = [
        "ptatfork.c",
    ];

    includes = [
        "$//apps/libc/include"
    ];

    lib = {
        "label": "libpthread_nonshared",
        "inputs": sources,
        "includes": includes,
    };

    entries = static_library(lib);
    return entries;
}

return build();
