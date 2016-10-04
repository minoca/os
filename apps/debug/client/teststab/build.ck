/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    STAB Test

Abstract:

    This program is used to test the symbol parser of the debug client.

Author:

    Evan Green 26-Jul-2012

Environment:

    Test

--*/

function build() {
    sources = [
        "teststab.c",
        "//debug/client:build/stabs.o",
        "//debug/client:build/coff.o",
        "//debug/client:build/elf.o",
        "//debug/client:build/symbols.o"
    ];

    build_libs = [
        "//lib/im:build_im",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc",
    ];

    build_app = {
        "label": "build_teststab",
        "output": "teststab",
        "inputs": sources + build_libs,
        "build": TRUE
    };

    entries = application(build_app);
    return entries;
}

return build();

