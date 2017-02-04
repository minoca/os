/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Lex/Parse LALR(1) Library

Abstract:

    This module implements support for an LALR(1) grammar parser generator.

Author:

    Evan Green 3-Feb-2017

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
        "lalr.c",
        "lr0.c",
        "output.c",
        "parcon.c",
        "verbose.c",
        "yygen.c"
    ];

    lib = {
        "label": "yygen",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_yygen",
        "output": "yy",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

