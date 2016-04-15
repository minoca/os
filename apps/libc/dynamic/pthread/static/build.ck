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

    sources_cppflags = [
        "$CPPFLAGS",
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": sources_cppflags
    };

    lib = {
        "label": "libpthread_nonshared",
        "inputs": sources,
        "sources_config": sources_config
    };

    entries = static_library(lib);
    return entries;
}

return build();
