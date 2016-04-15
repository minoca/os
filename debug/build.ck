/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Debug

Abstract:

    This directory builds the debugger application and supporting extensions.

Author:

    Evan Green 26-Jul-2012

Environment:

    Debug

--*/

function build() {
    debug_binaries = [
        "//debug/client:debug",
        "//debug/client:build_debug",
        "//debug/client/tdwarf:build_tdwarf",
        "//debug/client/testdisa:build_testdisa",
        "//debug/client/teststab:build_teststab",
        "//debug/kexts:kexts",
        "//debug/kexts:build_kexts"
    ];

    if (build_os == "Windows") {
        debug_binaries += [
            "//debug/client:build_debugui"
        ];
    }

    entries = group("debug", debug_binaries);
    return entries;
}

return build();
