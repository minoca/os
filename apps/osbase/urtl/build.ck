/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    User Mode Runtime

Abstract:

    This library contains the user-mode specific stubs needed by the core
    runtime library (the Rtl library).

Author:

    Evan Green 20-Feb-2013

Environment:

    User

--*/

function build() {
    sources = [
        "assert.c",
        "uprint.c"
    ];

    lib = {
        "label": "urtl",
        "inputs": sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
