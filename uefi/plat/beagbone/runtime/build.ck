/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

function build() {
    sources = [
        "reboot.c",
        "rtc.c",
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
        "label": "bbonert.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$///uefi/include/link_arm.x",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("bbonert");
    return entries;
}

return build();
