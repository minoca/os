/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    SSL Library

Abstract:

    This module contains a limited SSL support library.

Author:

    Evan Green 22-Jul-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "asn1.c",
        "base64.c",
        "bigint.c",
        "loader.c",
        "rsa.c"
    ];

    lib = {
        "label": "ssl",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_ssl",
        "output": "ssl",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
