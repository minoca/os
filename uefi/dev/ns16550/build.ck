/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    NS 16550 UART

Abstract:

    This library contains the NS 16550 UART controller library.

Author:

    Chris Stevens 10-Jul-2015

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
        "ns16550.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "ns16550",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

