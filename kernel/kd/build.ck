/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Kernel Debugger

Abstract:

    This library contains the support code necessary to enable debugging
    of a running kernel. This library is included in all versions of the
    operating system.

Author:

    Evan Green 10-Aug-2012

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "kdebug.c"
    ];

    boot_sources = [
        ":kdebug.o",
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/kdarch.c",
            "armv7/kdatomic.S",
            "armv7/kdsup.S",
            "armv7/kdsupc.c"
        ];

        boot_arch_sources = [
            ":armv7/kdarch.o",
            "boot/armv7/kdatomic.S",
            ":armv7/kdsup.o",
            ":armv7/kdsupc.o"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        arch_sources = [
            "x86/kdarch.c",
            "x86/kdsup.S"
        ];

        boot_arch_sources = [
            ":x86/kdarch.o",
            ":x86/kdsup.o"
        ];
    }

    lib = {
        "label": "kd",
        "inputs": base_sources + arch_sources,
    };

    boot_lib = {
        "label": "kdboot",
        "inputs": boot_sources + boot_arch_sources,
    };

    entries = static_library(lib);
    entries += static_library(boot_lib);
    return entries;
}

return build();
