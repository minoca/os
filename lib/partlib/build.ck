/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Partition Library

Abstract:

    This module implements support for disk partitions.

Author:

    Evan Green 30-Jan-2014

Environment:

    Any

--*/

function build() {
    sources = [
        "gpt.c",
        "partlib.c"
    ];

    lib = {
        "label": "partlib",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_partlib",
        "output": "partlib",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
