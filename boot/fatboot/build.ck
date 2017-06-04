/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    FATBoot

Abstract:

    This module implements the boot code between the MBR and the loader on
    BIOS systems. It knows just enough to load the Boot Manager. It runs
    in a very gray environment, is undebuggable, and should be considered
    sacred code.

Author:

    Evan Green 14-Oct-2013

Environment:

    Boot

--*/

from menv import staticApplication, flattenedBinary, mconfig;

function build() {
    var arch = mconfig.arch;
    var config;
    var entries;
    var flattened;
    var image;
    var includes;
    var sources;
    var sourcesConfig = {};

    sources = [
        "vbr.S",
        "fatboot.c",
        "prochw.c",
    ];

    if (arch == "x86") {
        sources += [
            "boot/lib:x86/archsup.o",
            "boot/lib:pcat/realmode.o",
            "boot/lib:pcat/x86/realmexe.o"
        ];

    } else if (arch == "x64") {
        sources += [
            "boot/lib:x6432/x86/archsup.o",
            "boot/lib:x6432/pcat/realmode.o",
            "boot/lib:x6432/pcat/x86/realmexe.o"
        ];
    }

    includes = [
        "$S/boot/lib/include",
        "$S/boot/lib/pcat"
    ];

    config = {
        "LDFLAGS": ["-Wl,-zmax-page-size=1"],
    };

    if (arch == "x64") {
        config["LDFLAGS"] += ["-m32"];
        sourcesConfig["CPPFLAGS"] = ["-m32"];
    }

    image = {
        "label": "fatboot.elf",
        "inputs": sources,
        "includes": includes,
        "config": config,
        "sources_config": sourcesConfig,
        "text_address": "0x7C00",
    };

    entries = staticApplication(image);

    //
    // Flatten the binary so it can be written directly to disk and loaded by
    // the MBR.
    //

    flattened = {
        "label": "fatboot.bin",
        "inputs": [":fatboot.elf"],
        "binplace": "bin",
        "nostrip": true
    };

    flattened = flattenedBinary(flattened);
    entries += flattened;
    return entries;
}

