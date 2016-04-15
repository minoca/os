/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Terminal Library

Abstract:

    This library contains the terminal support library.

Author:

    Evan Green 28-Jul-2014

Environment:

    Any

--*/

function build() {
    sources = [
        "term.c"
    ];

    lib = {
        "label": "termlib",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_termlib",
        "output": "termlib",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
