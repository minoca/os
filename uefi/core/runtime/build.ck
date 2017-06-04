/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    UEFI Runtime Core

Abstract:

    This module implements the UEFI core runtime support, which only
    supports setting the virtual address map. It is implemented as a
    separate runtime binary to avoid the paradox of ExitBootServices
    and SetVirtualAddressMap trying to tear down or change themselves.

Author:

    Evan Green 10-Mar-2014

Environment:

    Firmware

--*/

from menv import staticApplication, mconfig, uefiRuntimeFfs;

function build() {
    var arch = mconfig.arch;
    var elf;
    var entries;
    var includes;
    var libs;
    var linkConfig;
    var sources;
    var sourcesConfig;

    sources = [
        "uefi/core:crc32.o",
        "runtime.c",
    ];

    libs = [
        "uefi/archlib:uefiarch"
    ];

    includes = [
        "$S/uefi/include",
        "$S/uefi/core"
    ];

    sourcesConfig = {
        "CFLAGS": ["-fshort-wchar"],
    };

    linkConfig = {
        "LDFLAGS": ["-pie", "-nodefaultlibs", "-nostartfiles"]
    };

    elf = {
        "label": "rtbase.elf",
        "inputs": sources + libs,
        "sources_config": sourcesConfig,
        "includes": includes,
        "entry": "EfiRuntimeDriverEntry",
        "config": linkConfig
    };

    if ((arch == "armv7") || (arch == "armv6")) {
        elf["linker_script"] = "$S/uefi/include/link_arm.x";
    }

    entries = staticApplication(elf);
    entries += uefiRuntimeFfs("rtbase");
    return entries;
}

