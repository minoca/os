/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Image Library

Abstract:

    This library contains the Image library used to parse executable
    binaries.

Author:

    Evan Green 13-Oct-2012

Environment:

    Any

--*/

function build() {
    sources = [
        "elf.c",
        "elf64.c",
        "elfcomm.c",
        "image.c",
        "pe.c"
    ];

    lib = {
        "label": "im",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_im",
        "output": "im",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
