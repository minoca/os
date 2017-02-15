/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Chalk Library

Abstract:

    This library contains the Chalk interpreter, which provides a very
    basic programming language used to describe things like setup recipes
    and build configurations.

Author:

    Evan Green 19-Nov-2015

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
        "cflow.c",
        "cif.c",
        "const.c",
        "exec.c",
        "expr.c",
        "lang.c",
        "obj.c",
        "util.c"
    ];

    lib = {
        "label": "chalk",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_chalk",
        "output": "chalk",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

