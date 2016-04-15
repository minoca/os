/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    FAT Library

Abstract:

    This module implements support for the FAT file system.

Author:

    Evan Green 23-Sep-2012

Environment:

    Any

--*/

function build() {
    sources = [
        "fat.c",
        "fatcache.c",
        "fatsup.c",
        "idtodir.c"
    ];

    lib = {
        "label": "fat",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_fat",
        "output": "fat",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
