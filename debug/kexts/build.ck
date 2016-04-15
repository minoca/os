/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Kernel Debugging Extensions

Abstract:

    This module implements kernel debugger extensions.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug

--*/

function build() {
    sources = [
        "acpiext.c",
        "kexts.c",
        "memory.c",
        "objects.c",
        "reslist.c",
        "threads.c"
    ];

    target_libs = [
        "//debug/dbgext:dbgext"
    ];

    build_libs = [
        "//debug/dbgext:build_dbgext"
    ];

    lib = {
        "label": "kexts",
        "inputs": sources + target_libs,
    };

    build_lib = {
        "label": "build_kexts",
        "output": "kexts",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = shared_library(lib);
    entries += shared_library(build_lib);
    return entries;
}

return build();
