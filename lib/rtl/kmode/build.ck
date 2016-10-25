/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Kernel Rtl

Abstract:

    This library contains kernel-specific functions for the Runtime Library.

Author:

    Evan Green 26-Jul-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "assert.c",
        "kprint.c"
    ];

    includes = [
        "$//lib/rtl"
    ];

    lib = {
        "label": "krtl",
        "inputs": sources,
        "includes": includes
    };

    entries = static_library(lib);
    return entries;
}

return build();
