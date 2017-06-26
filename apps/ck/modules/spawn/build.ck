/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Spawn Module

Abstract:

    This directory builds the spawn module, which is used to launch other
    processes in Chalk.

Author:

    Evan Green 21-Jun-2017

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
        "spawn.c"
    ];

    posixSources = ["uos.c"];
    win32Sources = ["win32.c"];

    //
    // Create the static and dynamic versions of the module targeted at Minoca.
    //

    lib = {
        "label": "spawn_static",
        "output": "spawn",
        "inputs": commonSources + posixSources
    };

    objs = compiledSources(lib);
    entries = staticLibrary(lib);
    lib = {
        "label": "spawn_dynamic",
        "output": "spawn",
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
        "label": "build_spawn_static",
        "output": "spawn",
        "inputs": buildSources,
        "build": true,
        "prefix": "build"
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build_spawn_dynamic",
        "output": "spawn",
        "inputs": objs[0],
        "build": true,
        "prefix": "build"
    };

    entries += chalkSharedModule(lib);
    entries += group("all", [":spawn_static", ":spawn_dynamic"]);
    entries += group("build_all",
                     [":build_spawn_static", ":build_spawn_dynamic"]);

    return entries;
}

