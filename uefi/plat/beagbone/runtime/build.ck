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
        "label": "bbonert.elf",
        "inputs": sources + libs,
        "sources_config": sources_config,
        "includes": includes,
        "entry": "EfiRuntimeCoreEntry",
        "linker_script": "$//uefi/include/link_arm.x",
        "config": link_config
    };

    entries = executable(elf);
    entries += uefi_runtime_ffs("bbonert");
    return entries;
}

return build();
