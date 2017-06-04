/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ARM GIC Interrupt Controller

Abstract:

    This library contains the ARM Generic Interrupt Controller UEFI device.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

from menv import kernelLibrary;

function build() {
    var entries;
    var includes;
    var lib;
    var sources;
    var sourcesConfig;

    sources = [
        "gic.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "gic",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

