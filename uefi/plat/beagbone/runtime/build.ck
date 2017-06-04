/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    BeagleBone UEFI Runtime

Abstract:

    This module implements the BeagleBone runtime firmware, which continues
    to be loaded and provide services to the OS kernel even after the boot
    environment has been destroyed. It is never unloaded.

Author:

    Evan Green 6-Jan-2015

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
    var linkLdflags;
    var sources;
    var sourcesConfig;

    sources = [
        "reboot.c",
        "rtc.c",
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

    linkLdflags = [
        "-pie",
        "-Wl,--no-wchar-size-warning",
    ];

    linkConfig = {
        "LDFLAGS": linkLdflags
    };

    elf = {
        "label": "bbonert.elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$S/uefi/include/link_arm.x",
        "config": linkConfig
    };

    entries = staticApplication(elf);
    entries += uefiRuntimeFfs("bbonert");
    return entries;
}

