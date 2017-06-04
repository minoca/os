/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    OMAP UART

Abstract:

    This library contains the UART controller library used in Texas
    Instruments OMAP3 and OMAP4 SoCs.

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
        "omapuart.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "omapuart",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

