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

from menv import mconfig, kernelLibrary, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var buildLib;
    var entries;
    var lib;
    var lib32;
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

    entries = kernelLibrary(lib);
    entries += staticLibrary(buildLib);
    if (arch == "x64") {
        lib32 = {
            "label": "fat32",
            "inputs": sources,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(lib32);
    }

    return entries;
}

