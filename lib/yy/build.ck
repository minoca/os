/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Lex/Parse Library

Abstract:

    This module implements support for a simple lexer and parser. This is
    not the world's greatest implementation, but works for straightforward
    language specifications.

Author:

    Evan Green 9-Oct-2015

Environment:

    Any

--*/

function build() {
    sources = [
        "lex.c",
        "parse.c"
    ];

    lib = {
        "label": "yy",
        "inputs": sources,
    };

    build_lib = {
        "label": "build_yy",
        "output": "yy",
        "inputs": sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
