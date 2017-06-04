/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Boot Configuration Library

Abstract:

    This module implements the Boot Configuration Library.

Author:

    Evan Green 20-Feb-2014

Environment:

    Any

--*/

from menv import mconfig, kernelLibrary, staticLibrary;

function build() {
    var arch = mconfig.arch;
    var bconfLib;
    var bconfLib32;
    var buildBconfLib;
    var entries;
    var sources;

    sources = [
        "bconf.c"
    ];

    bconfLib = {
        "label": "bconf",
        "inputs": sources,
    };

    buildBconfLib = {
        "label": "build_bconf",
        "output": "bconf",
        "inputs": sources,
        "build": true,
        "prefix": "build"
    };

    entries = kernelLibrary(bconfLib);
    entries += staticLibrary(buildBconfLib);
    if (arch == "x64") {
        bconfLib32 = {
            "label": "bconf32",
            "inputs": sources,
            "prefix": "x6432",
            "sources_config": {"CPPFLAGS": "-m32"}
        };

        entries += kernelLibrary(bconfLib32);
    }

    return entries;
}

