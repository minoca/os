/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Kernel

Abstract:

    This is the core of the operating system.

Author:

    Evan Green 26-Jul-2012

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "init.c"
    ];

    boot_arch_sources = [];
    if ((arch == "armv7") || (arch == "armv6")) {
        arm_sources = [
            "armv7/commsup.S",
            "armv7/inttable.S",
            "armv7/prochw.c",
            "armv7/sstep.c",
            "armv7/trap.S",
            "armv7/vfp.c"
        ];

        boot_arch_sources = [":armv7/sstep.o"];
        if (arch == "armv7") {
            arch_sources = arm_sources + [
                "armv7/archsup.S",
                "armv7/archsupc.c"
            ];

        } else {
            arch_sources = arm_sources + [
                "armv6/archsup.S",
                "armv6/archsupc.c"
            ];
        }

    } else if (arch == "x86") {
        arch_sources = [
            "x86/archsup.S",
            "x86/archsupc.c",
            "x86/prochw.c",
            "x86/trap.S"
        ];
    }

    kernel_libs = [
        "//kernel/acpi:acpi",
        "//lib/crypto:crypto",
        "//kernel/ob:ob",
        "//lib/rtl/base:basertl",
        "//lib/rtl/kmode:krtl",
        "//lib/im:im",
        "//lib/basevid:basevid",
        "//lib/termlib:termlib",
        "//kernel/kd:kd",
        "//kernel/kd/kdusb:kdusb",
        "//kernel/ps:ps",
        "//kernel/ke:ke",
        "//kernel/io:io",
        "//kernel/hl:hl",
        "//kernel/mm:mm",
        "//kernel/sp:sp"
    ];

    kernel_config = {
        "LDFLAGS": ["-Wl,--whole-archive"]
    };

    kernel = {
        "label": "kernel",
        "inputs": base_sources + arch_sources + kernel_libs,
        "entry": "KepStartSystem",
        "config": kernel_config
    };

    boot_arch_lib = {
        "label": "archboot",
        "inputs": boot_arch_sources
    };

    entries = driver(kernel);
    if (boot_arch_sources) {
        entries += static_library(boot_arch_lib);
    }

    return entries;
}

return build();

