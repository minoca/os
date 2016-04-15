/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Crypt Library

Abstract:

    This module contains the crypt library, which contains the C library
    functions like crypt, encrypt, fcrypt, and setkey.

Author:

    Evan Green 6-Mar-2015

Environment:

    User

--*/

function build() {
    sources = [
        "crypt.c"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS", "-I$///apps/include"],
    };

    dynlibs = [
        "//apps/libc/dynamic:libc"
    ];

    so = {
        "label": "libcrypt",
        "inputs": sources + dynlibs,
        "sources_config": sources_config,
        "major_version": "1"
    };

    entries = shared_library(so);
    return entries;
}

return build();

