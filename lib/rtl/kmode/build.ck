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

from menv import mconfig, kernelLibrary;

function build() {
    var arch = mconfig.arch;
    var entries;
    var includes;
    var lib;
    var lib32;
    var sources;

    sources = [
        "assert.c",
        "kprint.c",
        "pdouble.c"
    ];

    includes = [
        "$S/lib/rtl"
    ];

    lib = {
        "label": "krtl",
        "inputs": sources,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    if (arch == "x64") {
        lib32 = {
            "label": "krtl32",
            "inputs": sources,
            "includes": includes,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(lib32);
    }

    return entries;
}

