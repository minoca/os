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
    var linkLdflags;
    var sources;
    var sources_config;

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

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    linkLdflags = [
        "-pie",
        "-nostdlib",
        "-Wl,--no-wchar-size-warning",
        "-static"
    ];

    linkConfig = {
        "LDFLAGS": linkLdflags
    };

    elf = {
        "label": "rpi2rt.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$S/uefi/include/link_arm.x",
        "config": linkConfig
    };

    entries = executable(elf);
    entries += uefiRuntimeFfs("rpi2rt");
    return entries;
}

