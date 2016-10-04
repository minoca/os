/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    DWARF Test

Abstract:

    This program is used to test the DWARF symbol parser in isolation.

Author:

    Evan Green 2-Dec-2015

Environment:

    Test

--*/

function build() {
    sources = [
        "tdwarf.c",
        "//debug/client:build/coff.o",
        "//debug/client:build/elf.o",
        "//debug/client:build/dwarf.o",
        "//debug/client:build/dwexpr.o",
        "//debug/client:build/dwframe.o",
        "//debug/client:build/dwline.o",
        "//debug/client:build/dwread.o",
        "//debug/client:build/stabs.o",
        "//debug/client:build/symbols.o"
    ];

    build_libs = [
        "//lib/im:build_im",
        "//lib/rtl/base:build_basertl",
        "//lib/rtl/rtlc:build_rtlc",
    ];

    build_app = {
        "label": "build_tdwarf",
        "output": "tdwarf",
        "inputs": sources + build_libs,
        "build": TRUE
    };

    entries = application(build_app);
    return entries;
}

return build();

