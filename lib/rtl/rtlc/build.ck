/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    RTL C

Abstract:

    This library contains stub functions to allow the Rtl Library to
    run on top of a standard C library.

Author:

    Evan Green 23-Oct-2012

Environment:

    Any

--*/

function build() {
    sources = [
        "stubs.c"
    ];

    includes = [
        "$//lib/rtl"
    ];

    lib = {
        "label": "rtlc",
        "inputs": sources,
        "includes": includes,
    };

    build_lib = {
        "label": "build_rtlc",
        "output": "rtlc",
        "inputs": sources,
        "includes": includes,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(build_lib);
    return entries;
}

return build();

