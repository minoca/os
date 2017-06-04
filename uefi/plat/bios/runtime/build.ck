/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PC/AT UEFI Runtime firmware

Abstract:

    This module implements the PC/AT BIOS runtime firmware, which continues
    to run even after ExitBootServices. It is never unloaded.

Author:

    Evan Green 18-Mar-2014

Environment:

    Firmware

--*/

from menv import staticApplication, uefiRuntimeFfs;

function build() {
    var elf;
    var entries;
    var includes;
    var libs;
    var linkConfig;
    var sources;
    var sourcesConfig;

    sources = [
        "reboot.c",
        "runtime.c"
    ];

    libs = [
        "uefi/core/rtlib:rtlib",
        "uefi/archlib:uefiarch"
    ];

    includes = [
        "$S/uefi/include"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    linkConfig = {
        "LDFLAGS": ["-pie"]
    };

    elf = {
        "label": "biosrt.elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "config": linkConfig
    };

    entries = staticApplication(elf);
    entries += uefiRuntimeFfs("biosrt");
    return entries;
}

