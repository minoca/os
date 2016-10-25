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

function build() {
    sources = [
        "fptest.c",
        "heaptest.c",
        "testrtl.c",
        "timetest.c"
    ];

    build_libs = [
        "//lib/rtl/rtlc:build_rtlc",
        "//lib/rtl/base:build_basertl"
    ];

    build_app = {
        "label": "build_testrtl",
        "output": "testrtl",
        "inputs": sources + build_libs,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();

