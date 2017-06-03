/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    TestRtl

Abstract:

    This program compiles the Rtl Library into an application and tests it.

Author:

    Evan Green 26-Jul-2012

Environment:

    Test

--*/

from menv import application;

function build() {
    var buildApp;
    var buildLibs;
    var entries;
    var sources;

    sources = [
        "fpstest.c",
        "fptest.c",
        "heaptest.c",
        "testrtl.c",
        "timetest.c"
    ];

    buildLibs = [
        "lib/rtl/base:build_basertl",
        "lib/rtl/urtl:build_rtlc"
    ];

    buildApp = {
        "label": "build_testrtl",
        "output": "testrtl",
        "inputs": sources + buildLibs,
        "build": true,
        "prefix": "build"
    };

    entries = application(buildApp);
    return entries;
}

