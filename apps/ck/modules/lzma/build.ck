/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    LZMA Module

Abstract:

    This directory builds the LZMA module, which enables compressing and
    decompressing Minoca LZMA streams.

Author:

    Evan Green 22-May-2017

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
        "entry.c",
        "lzma.c"
    ];

    //
    // Create the static and dynamic versions of the module targeted at Minoca.
    //

    lib = {
        "label": "lzma_static",
        "output": "lzma",
        "inputs": commonSources + ["apps/lib/lzma:liblzma"]
    };

    objs = compiledSources(lib);
    entries = staticLibrary(lib);
    lib = {
        "label": "lzma_dynamic",
        "output": "lzma",
        "inputs": objs[0]
    };

    entries += chalkSharedModule(lib);
    lib = {
        "label": "build_lzma_static",
        "output": "lzma",
        "inputs": commonSources + ["apps/lib/lzma:build_liblzma"],
        "build": true,
        "prefix": "build"
    };

    objs = compiledSources(lib);
    entries += staticLibrary(lib);
    lib = {
        "label": "build_lzma_dynamic",
        "output": "lzma",
        "inputs": objs[0],
        "build": true,
        "prefix": "build"
    };

    entries += chalkSharedModule(lib);
    entries += group("all", [":lzma_static", ":lzma_dynamic"]);
    entries += group("build_all",
                     [":build_lzma_static", ":build_lzma_dynamic"]);

    return entries;
}

