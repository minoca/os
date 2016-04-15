/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Chalk Library

Abstract:

    This library contains the Chalk interpreter, which provides a very
    basic programming language used to describe things like setup recipes
    and build configurations.

Author:

    Evan Green 19-Nov-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "cflow.c",
        "cif.c",
        "const.c",
        "exec.c",
        "expr.c",
        "lang.c",
        "obj.c",
        "util.c"
    ];

    lib = {
        "label": "chalk",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_chalk",
        "output": "chalk",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();

