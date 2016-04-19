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

    includes = [
        "$//apps/include"
    ];

    dynlibs = [
        "//apps/libc/dynamic:libc"
    ];

    so = {
        "label": "libcrypt",
        "inputs": sources + dynlibs,
        "includes": includes,
        "major_version": "1"
    };

    entries = shared_library(so);
    return entries;
}

return build();

