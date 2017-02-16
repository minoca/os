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

from menv import application;

function build() {
    var buildApp;
    var buildLibs;
    var config;
    var entries;
    var includes;
    var sources;

    sources = [
        "bsrchtst.c",
        "getoptst.c",
        "mathtst.c",
        "mathftst.c",
        "qsorttst.c",
        "regextst.c",
        "testc.c"
    ];

    buildLibs = [
        "apps/libc/dynamic:build_libc",
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    config = {
        "CFLAGS": ["-ffreestanding"]
    };

    buildApp = {
        "label": "build_testc",
        "output": "testc",
        "inputs": sources + buildLibs,
        "includes": includes,
        "build": true,
        "prefix": "build",
        "sources_config": config
    };

    entries = application(buildApp);
    return entries;
}

