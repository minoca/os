/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Base Video Library

Abstract:

    This module implements basic support for video output via a linear
    framebuffer.


Author:

    Evan Green 30-Jan-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "fontdata.c",
        "textvid.c"
    ];

    lib = {
        "label": "basevid",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_basevid",
        "output": "basevid",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
