/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Static pthread

Abstract:

    This module implements functions that must be statically linked for the
    POSIX thread library.

Author:

    Evan Green 4-May-2015

Environment:

    User Mode C Library

--*/

from menv import staticLibrary;

function build() {
    var entries;
    var includes;
    var lib;
    var sources;

    sources = [
        "ptatfork.c",
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    lib = {
        "label": "libpthread_nonshared",
        "inputs": sources,
        "includes": includes,
    };

    entries = staticLibrary(lib);
    return entries;
}

