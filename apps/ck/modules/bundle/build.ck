/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Bundle Module

Abstract:

    This directory builds the bundle module, a static-only module that must be
    built into the app, and can provide executable repackaging functionality.
    This allows Chalk to create single-executable programs that execute an
    embedded script.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import group, staticLibrary;

function build() {
    var all;
    var entries;
    var lib;
    var sources;

    sources = [
        "bundle.c"
    ];

    lib = {
        "label": "bundle",
        "inputs": sources
    };

    entries = staticLibrary(lib);
    lib = {
        "label": "build_bundle",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries += staticLibrary(lib);
    entries += group("all", [":bundle"]);
    entries += group("build_all", [":build_bundle"]);
    return entries;
}

