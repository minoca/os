/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    OS Time Module

Abstract:

    This directory builds the OS time module, which exposes interfaces to the
    operating system's underlying time functionality.

Author:

    Evan Green 7-Jun-2017

Environment:

    C

--*/

from menv import compiledSources, group, mconfig, staticLibrary;
from apps.ck.modules.build import chalkSharedModule;

function build() {
    var buildOs = mconfig.build_os;
    var buildSources;
    var commonSources;
    var lib;
    var entries;
    var objs;
    var posixSources;
    var win32Sources;

    commonSources = [
        "entry.c",
        "time.c"
    ];

    posixSources = [];
    win32Sources = ["win32.c"];

    //
    // Create the static and dynamic versions of the module targeted at Minoca.
    //

    lib = {
        "label": "_time_static",
        "output": "_time",
        "inputs": commonSources + posixSources
    };

    objs = compiledSources(lib);
    entries = staticLibrary(lib);
    lib = {
        "label": "_time_dynamic",
        "output": "_time",
        "inputs": objs[0]
    };

    entries += chalkSharedModule(lib);

    //
    // Create the static and dynamic versions of the module for the build
    // machine.
    //

    if (buildOs == "Windows") {
        buildSources = commonSources + win32Sources;

    } else {
        buildSources = commonSources + posixSources;
    }

    lib = {
        "label": "build__time_static",
        "output": "_time",
        "inputs": buildSources,
        "build": true,
        "prefix": "build"
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build__time_dynamic",
        "output": "_time",
        "inputs": objs[0],
        "build": true,
        "prefix": "build"
    };

    entries += chalkSharedModule(lib);
    entries += group("all", [":_time_static", ":_time_dynamic"]);
    entries += group("build_all",
                     [":build__time_static", ":build__time_dynamic"]);

    return entries;
}

