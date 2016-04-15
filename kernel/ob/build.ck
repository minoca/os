/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Object Manager

Abstract:

    This library contains the Object Manager. It maintains a global
    file-system like hierarchy of objects used by many other kernel
    components including Mm, Io, Ke and Ps.

Author:

    Evan Green 4-Sep-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "handles.c",
        "obapi.c"
    ];

    lib = {
        "label": "ob",
        "inputs": sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
