/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FAT Library

Abstract:

    This module implements support for the FAT file system.

Author:

    Evan Green 23-Sep-2012

Environment:

    Any

--*/

from menv import staticLibrary;

function build() {
    var buildLib;
    var entries;
    var lib;
    var sources;

    sources = [
        "fat.c",
        "fatcache.c",
        "fatsup.c",
        "idtodir.c"
    ];

    lib = {
        "label": "fat",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_fat",
        "output": "fat",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

