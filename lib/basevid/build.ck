/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Base Video Library

Abstract:

    This module implements basic support for video output via a linear
    framebuffer.


Author:

    Evan Green 30-Jan-2015

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
        "fontdata.c",
        "textvid.c"
    ];

    lib = {
        "label": "basevid",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_basevid",
        "output": "basevid",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

