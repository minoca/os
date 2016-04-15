/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Boot Configuration Library

Abstract:

    This module implements the Boot Configuration Library.

Author:

    Evan Green 20-Feb-2014

Environment:

    Any

--*/

function build() {
    sources = [
        "bconf.c"
    ];

    bconf_lib = {
        "label": "bconf",
        "inputs": sources,
    };

    build_bconf_lib = {
        "label": "build_bconf",
        "output": "bconf",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(bconf_lib);
    entries += static_library(build_bconf_lib);
    return entries;
}

return build();
