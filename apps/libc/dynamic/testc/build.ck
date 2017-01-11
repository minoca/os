/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    C Library Test

Abstract:

    This program tests the independent portions of the C library.

Author:

    Evan Green 9-Jul-2013

Environment:

    Test

--*/

function build() {
    sources = [
        "bsrchtst.c",
        "getoptst.c",
        "mathtst.c",
        "mathftst.c",
        "qsorttst.c",
        "regextst.c",
        "testc.c"
    ];

    build_libs = [
        "//apps/libc/dynamic:build_libc",
    ];

    includes = [
        "$//apps/libc/include"
    ];

    build_app = {
        "label": "build_testc",
        "output": "testc",
        "inputs": sources + build_libs,
        "includes": includes,
        "build": TRUE,
        "prefix": "build"
    };

    entries = application(build_app);
    return entries;
}

return build();

