/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    LZMA Library

Abstract:

    This module implements Lempel-Ziv-Markov chain compression.

Author:

    Evan Green 23-May-2017

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
        "crc32.c",
        "encopt.c",
        "lzfind.c",
        "lzmadec.c",
        "lzmaenc.c"
    ];

    lib = {
        "label": "liblzma",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_liblzma",
        "output": "liblzma",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

