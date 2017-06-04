/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PL-031 RTC

Abstract:

    This library contains the ARM PrimeCell PL-031 Real Time Clock
    device support.

Author:

    Evan Green 7-Apr-2014

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
        "pl031.c"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "pl031",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

