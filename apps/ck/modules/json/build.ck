/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    JSON Module

Abstract:

    This directory builds the JSON module, which enables dumping and loading of
    objects in Javascript Object Notation.

Author:

    Evan Green 19-May-2017

Environment:

    C

--*/

from menv import compiledSources, group, mconfig, staticLibrary;
from apps.ck.modules.build import chalkSharedModule;

function build() {
    var buildOs = mconfig.build_os;
    var commonSources;
    var lib;
    var entries;
    var objs;

    commonSources = [
        "decode.c",
        "encode.c",
        "entry.c",
        "json.c"
    ];

    //
    // Create the static and dynamic versions of the module targeted at Minoca.
    //

    lib = {
        "label": "json_static",
        "output": "json",
        "inputs": commonSources
    };

    objs = compiledSources(lib);
    entries = staticLibrary(lib);
    lib = {
        "label": "json_dynamic",
        "output": "json",
        "inputs": objs[0]
    };

    entries += chalkSharedModule(lib);
    lib = {
        "label": "build_json_static",
        "output": "json",
        "inputs": commonSources,
        "build": true,
        "prefix": "build"
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build_json_dynamic",
        "output": "json",
        "inputs": objs[0],
        "build": true,
        "prefix": "build"
    };

    entries += chalkSharedModule(lib);
    entries += group("all", [":json_static", ":json_dynamic"]);
    entries += group("build_all",
                     [":build_json_static", ":build_json_dynamic"]);

    return entries;
}

