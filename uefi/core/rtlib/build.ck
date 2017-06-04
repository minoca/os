/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    UEFI Runtime Library

Abstract:

    This file is responsible for building the core UEFI Runtime support,
    which is compiled into most platform runtime images.

Author:

    Evan Green 18-Mar-2014

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
        "uefi/core:crc32.o",
        "uefi/core:div.o",
        "init.c",
        "time.c",
        "uefi/core:util.o",
        "var.c"
    ];

    includes = [
        "$S/uefi/include",
        "$S/uefi/core"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "rtlib",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

