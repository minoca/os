/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Partition Library

Abstract:

    This module implements support for disk partitions.

Author:

    Evan Green 30-Jan-2014

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
        "gpt.c",
        "partlib.c"
    ];

    lib = {
        "label": "partlib",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_partlib",
        "output": "partlib",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = kernelLibrary(lib);
    entries += staticLibrary(buildLib);
    if (arch == "x64") {
        lib32 = {
            "label": "partlib32",
            "inputs": sources,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": ["-m32"]}
        };

        entries += kernelLibrary(lib32);
    }

    return entries;
}

