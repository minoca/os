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

function build() {
    sources = [
        "//uefi/core:crc32.o",
        "runtime.c",
    ];

    libs = [
        "//uefi/archlib:uefiarch"
    ];

    includes = [
        "$//uefi/include",
        "$//uefi/core"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    link_config = {
        "LDFLAGS": ["-pie", "-nostdlib", "-static"]
    };

    elf = {
        "label": "rtbase.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "entry": "EfiRuntimeDriverEntry",
        "config": link_config
    };

    if ((arch == "armv7") || (arch == "armv6")) {
        elf["linker_script"] = "$//uefi/include/link_arm.x";
    }

    entries = executable(elf);
    entries += uefi_runtime_ffs("rtbase");
    return entries;
}

return build();
