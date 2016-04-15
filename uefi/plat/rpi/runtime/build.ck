/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

function build() {
    sources = [
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
        "-Wl,--no-wchar-size-warning",
        "-static"
    ];

    link_config = {
        "LDFLAGS": ["$LDFLAGS"] + link_ldflags
    };

    elf = {
        "label": "rpirt.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$///uefi/include/link_arm.x",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("rpirt");
    return entries;
}

return build();
