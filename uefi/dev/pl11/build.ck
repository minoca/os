/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PL-011 UART

Abstract:

    This library contains the ARM PrimeCell PL-011 UART controller library.

Author:

    Evan Green 27-Feb-2014

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
        "pl11.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "pl11",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

