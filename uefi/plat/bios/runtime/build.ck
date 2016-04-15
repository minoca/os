/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
        "-I$///uefi/include"
    ];

    sources_config = {
        "CFLAGS": ["$CFLAGS", "-fshort-wchar"],
        "CPPFLAGS": ["$CPPFLAGS"] + includes
    };

    link_ldflags = [
        "-pie",
        "-nostdlib",
        "-static"
    ];

    link_config = {
        "LDFLAGS": ["$LDFLAGS"] + link_ldflags
    };

    elf = {
        "label": "biosrt.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "entry": "EfiRuntimeCoreEntry",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("biosrt");
    return entries;
}

return build();
