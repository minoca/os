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

from menv import staticLibrary;

function build() {
    var buildLib;
    var entries;
    var includes;
    var lib;
    var sources;

    sources = [
        "stubs.c"
    ];

    includes = [
        "$S/lib/rtl"
    ];

    lib = {
        "label": "rtlc",
        "inputs": sources,
        "includes": includes,
    };

    buildLib = {
        "label": "build_rtlc",
        "output": "rtlc",
        "inputs": sources,
        "includes": includes,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

