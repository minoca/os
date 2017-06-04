/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Architecture Support

Abstract:

    This module contains architecure-specific UEFI core support functions.

Author:

    Evan Green 27-Mar-2014

Environment:

    Firmware

--*/

from menv import mconfig, kernelLibrary;

function build() {
    var arch = mconfig.arch;
    var armSources;
    var armv6Sources;
    var armv7Sources;
    var entries;
    var includes;
    var lib;
    var sources;
    var sourcesConfig;
    var x86Sources;

    x86Sources = [
        "x86/archlib.c",
        "x86/archsup.S",
        "x86/ioport.S",
        "x86/regacces.c"
    ];

    armSources = [
        "armv7/archlib.c",
        "armv7/regacces.c"
    ];

    armv7Sources = armSources + [
        "armv7/archsup.S"
    ];

    armv6Sources = armSources + [
        "armv6/archsup.S"
    ];

    if (arch == "armv7") {
        sources = armv7Sources;

    } else if (arch == "armv6") {
        sources = armv6Sources;

    } else if (arch == "x86") {
        sources = x86Sources;

    } else {
        Core.raise(ValueError("Unknown Architecture"));
    }

    includes = [
        "$S/uefi/include",
        "$S/uefi/core"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    lib = {
        "label": "uefiarch",
        "inputs": sources,
        "sources_config": sourcesConfig,
        "includes": includes
    };

    entries = kernelLibrary(lib);
    return entries;
}

