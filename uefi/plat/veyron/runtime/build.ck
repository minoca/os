/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Veyron UEFI Runtime

Abstract:

    This module implements the RK3288 Veyron runtime firmware, which
    continues to be loaded and provide services to the OS kernel even
    after the boot environment has been destroyed. It is never unloaded.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

function build() {
    sources = [
        "i2c.c",
        "pmic.c",
        "reboot.c",
        "rtc.c",
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
        "-Wl,--no-wchar-size-warning",
        "-static"
    ];

    link_config = {
        "LDFLAGS": link_ldflags
    };

    elf = {
        "label": "veyronrt.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$//uefi/include/link_arm.x",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("veyronrt");
    return entries;
}

return build();
