/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Static C Library

Abstract:

    This module contains the portion of the C library that is statically
    linked into every application. It contains little other than some
    initialization stubs.

Author:

    Evan Green 4-Mar-2013

Environment:

    User Mode C Library

--*/

from menv import mconfig, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var entries;
    var includes;
    var lib;
    var sources;

    sources = [
        "init.c",
        "atexit.c",
        "dlsym.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        archSources = [
            "armv7/aatexit.c",
            "armv7/crt0.S"
        ];

    } else if (arch == "x86") {
        archSources = [
            "x86/crt0.S"
        ];

    } else if (arch == "x64") {
        archSources = [
            "x64/crt0.S"
        ];
    }

    includes = [
        "$S/apps/libc/include"
    ];

    lib = {
        "label": "libc_nonshared",
        "inputs": archSources + sources,
        "includes": includes
    };

    entries = staticLibrary(lib);
    return entries;
}

