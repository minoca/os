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

function build() {
    sources = [
        "reboot.c",
        "runtime.c"
    ];

    libs = [
        "//uefi/core/rtlib:rtlib",
        "//uefi/archlib:uefiarch"
    ];

    includes = [
        "$//uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["-fshort-wchar"],
    };

    link_ldflags = [
        "-pie",
        "-nostdlib",
        "-static"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    elf = {
        "label": "biosrt.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("biosrt");
    return entries;
}

return build();
