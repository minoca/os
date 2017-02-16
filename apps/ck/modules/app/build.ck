/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    App Module

Abstract:

    This directory builds the app module, a static-only module that must be
    built into the app, and can provide application-level details.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import group, staticLibrary;

function build() {
    var entries;
    var lib;
    var sources;

    sources = [
        "app.c"
    ];

    lib = {
        "label": "app",
        "inputs": sources
    };

    entries = staticLibrary(lib);
    lib = {
        "label": "build_app",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries += staticLibrary(lib);
    entries += group("all", [":app"]);
    entries += group("build_all", [":build_app"]);
    return entries;
}

