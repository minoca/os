/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    DbgExt

Abstract:

    This module implements the import library for kernel debugger
    extensions.

Author:

    Evan Green 8-May-2013

Environment:

    Debug

--*/

function build() {
    sources = [
        "extimp.c",
    ];

    lib = {
        "label": "dbgext",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_dbgext",
        "output": "dbgext",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
