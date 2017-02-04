/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Image Library

Abstract:

    This library contains the Image library used to parse executable
    binaries.

Author:

    Evan Green 13-Oct-2012

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
        "elf.c",
        "elf64.c",
        "elfcomm.c",
        "image.c",
        "pe.c"
    ];

    lib = {
        "label": "im",
        "inputs": sources,
    };

    buildLib = {
        "label": "build_im",
        "output": "im",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    return entries;
}

