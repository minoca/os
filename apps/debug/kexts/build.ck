/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Kernel Debugging Extensions

Abstract:

    This module implements kernel debugger extensions.

Author:

    Evan Green 10-Sep-2012

Environment:

    Debug

--*/

from menv import sharedLibrary;

function build() {
    var buildLib;
    var buildLibs;
    var entries;
    var lib;
    var sources;
    var targetLibs;

    sources = [
        "acpiext.c",
        "kexts.c",
        "memory.c",
        "objects.c",
        "reslist.c",
        "threads.c"
    ];

    targetLibs = [
        "apps/debug/dbgext:dbgext"
    ];

    buildLibs = [
        "apps/debug/dbgext:build_dbgext"
    ];

    lib = {
        "label": "kexts",
        "inputs": sources + targetLibs,
    };

    buildLib = {
        "label": "build_kexts",
        "output": "kexts",
        "inputs": sources + buildLibs,
        "build": true,
        "prefix": "build",
        "binplace": "tools/bin",
        "nostrip": true
    };

    entries = sharedLibrary(lib);
    entries += sharedLibrary(buildLib);
    return entries;
}

