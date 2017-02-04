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

from menv import staticLibrary;

function build() {
    var buildLib;
    var entries;
    var lib;
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

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

