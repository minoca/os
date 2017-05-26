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

from menv import mconfig, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var buildLib;
    var entries;
    var lib;
    var nativeLib;
    var nativeSources;
    var universalSources;

    universalSources = [
        "elf.c",
        "elf64.c",
        "elfcomm.c",
        "image.c",
        "imuniv.c",
        "pe.c"
    ];

    nativeSources = [
        ":elfcomm.o",
        "imnative.c",
    ];

    if ((arch == "armv6") || (arch == "armv7") || (arch == "x86")) {
        nativeSources += [":elf.o"];

    } else if (arch == "x64") {
        nativeSources += [":elf64.o"];
    }

    lib = {
        "label": "imu",
        "inputs": universalSources,
    };

    nativeLib = {
        "label": "imn",
        "inputs": nativeSources
    };

    buildLib = {
        "label": "build_imu",
        "output": "imu",
        "inputs": universalSources,
        "build": true,
        "prefix": "build"
    };

    entries = staticLibrary(lib);
    entries += staticLibrary(buildLib);
    entries += staticLibrary(nativeLib);
    return entries;
}

