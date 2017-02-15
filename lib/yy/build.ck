/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Lex/Parse Library

Abstract:

    This module implements support for a simple lexer and parser. This is
    not the world's greatest implementation, but works for straightforward
    language specifications.

Author:

    Evan Green 9-Oct-2015

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
        "lex.c",
        "parse.c",
        "parser.c"
    ];

    lib = {
        "label": "yy",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_yy",
        "output": "yy",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

