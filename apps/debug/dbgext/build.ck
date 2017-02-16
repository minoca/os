/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    DbgExt

Abstract:

    This module implements the import library for kernel debugger
    extensions.

Author:

    Evan Green 8-May-2013

Environment:

    Debug

--*/

from menv import staticLibrary;

function build() {
    var buildLib;
    var entries;
    var lib;
    var sources;

    sources = [
        "extimp.c",
    ];

    lib = {
        "label": "dbgext",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_dbgext",
        "output": "dbgext",
        "inputs": sources,
        "build": true,
        "prefix": "build",
        "binplace": "tools/lib",
        "nostrip": true
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

