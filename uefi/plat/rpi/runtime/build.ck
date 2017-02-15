/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Raspberry Pi 2 UEFI Runtime

Abstract:

    This module implements the Raspberry Pi 2 runtime firmware, which
    continues running even after boot services have been torn down. It is
    never unloaded.

Author:

    Chris Stevens 17-Mar-2015

Environment:

    Firmware

--*/

from menv import executable, uefiRuntimeFfs;

function build() {
    var elf;
    var entries;
    var includes;
    var libs;
    var linkConfig;
    var LinkLdflags;
    var sources;
    var sourcesConfig;

    sources = [
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

    LinkLdflags = [
        "-pie",
        "-nostdlib",
        "-Wl,--no-wchar-size-warning",
        "-static"
    ];

    linkConfig = {
        "LDFLAGS": LinkLdflags
    };

    elf = {
        "label": "rpirt.elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$S/uefi/include/link_arm.x",
        "config": linkConfig
    };

    entries = executable(elf);
    entries += uefiRuntimeFfs("rpirt");
    return entries;
}

