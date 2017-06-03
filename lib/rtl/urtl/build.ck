/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import staticLibrary;

function build() {
    var buildRtlc;
    var entries;
    var includes;
    var lib;
    var rtlc;
    var rtlcSources;
    var sources;

    includes = [
        "$S/lib/rtl"
    ];

    sources = [
        "assert.c",
        ":pdouble.o",
        "uprint.c"
    ];

    rtlcSources = [
        "pdouble.c",
        "rtlc/stubs.c"
    ];

    lib = {
        "label": "urtl",
        "includes": includes,
        "inputs": sources,
    };

    rtlc = {
        "label": "rtlc",
        "includes": includes,
        "inputs": rtlcSources,
    };

    buildRtlc = {
        "label": "build_rtlc",
        "includes": includes,
        "inputs": rtlcSources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(rtlc);
    entries += staticLibrary(buildRtlc);
    return entries;
}

