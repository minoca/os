/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    OS Module

Abstract:

    This directory builds the OS module, which is a module that exposes the
    functionality of the underlying operating system to Chalk modules.

Author:

    Evan Green 14-Feb-2017

Environment:

    C

--*/

from menv import compiledSources, group, mconfig, staticLibrary;
from apps.ck.modules.build import chalkSharedModule;

function build() {
    var buildOs = mconfig.build_os;
    var buildConfig = {};
    var buildSources;
    var commonSources;
    var lib;
    var entries;
    var objs;
    var win32Sources;

    commonSources = [
        "entry.c",
        "errno.c",
        "io.c",
        "os.c",
        "osinfo.c"
    ];

    win32Sources = [
        "oswin32.c"
    ];

    //
    // Create the static and dynamic versions of the module targeted at Minoca.
    //

    lib = {
        "label": "os_static",
        "output": "os",
        "inputs": commonSources
    };

    objs = compiledSources(lib);
    entries = staticLibrary(lib);
    lib = {
        "label": "os_dynamic",
        "output": "os",
        "inputs": objs[0]
    };

    entries += chalkSharedModule(lib);

    //
    // Create the static and dynamic versions of the module for the build
    // machine.
    //

    if (buildOs == "Windows") {
        buildSources = commonSources + win32Sources;
        buildConfig["DYNLIBS"] = ["-lws2_32"];

    } else {
        buildSources = commonSources;
    }

    lib = {
        "label": "build_os_static",
        "output": "os",
        "inputs": buildSources,
        "build": true,
        "prefix": "build"
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build_os_dynamic",
        "output": "os",
        "inputs": objs[0],
        "build": true,
        "config": buildConfig,
        "prefix": "build"
    };

    entries += chalkSharedModule(lib);
    entries += group("all", [":os_static", ":os_dynamic"]);
    entries += group("build_all", [":build_os_static", ":build_os_dynamic"]);
    return entries;
}

