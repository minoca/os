/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Memory Manager

Abstract:

    This library contains the Memory Management library used by the kernel.
    It handles everything related to memory in the kernel, and meshes
    closely with the I/O and process libraries to provide fast and
    efficient management of memory resources.

Author:

    Evan Green 27-Jul-2012

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "block.c",
        "imgsec.c",
        "info.c",
        "init.c",
        "invipi.c",
        "iobuf.c",
        "load.c",
        "mdl.c",
        "paging.c",
        "physical.c",
        "kpools.c",
        "virtual.c",
        "fault.c"
    ];

    boot_sources = [
        ":mdl.o"
    ];

    arm_sources = [
        "armv7/archcomc.c",
        "armv7/flush.c",
        "armv7/mapping.c",
        "armv7/usermem.S"
    ];

    armv7_sources = arm_sources + [
        "armv7/archsupc.c"
    ];

    armv6_sources = arm_sources + [
        "armv6/archsupc.c"
    ];

    x86_sources = [
        "x86/archsupc.c",
        "x86/flush.c",
        "x86/mapping.c",
        "x86/usermem.S"
    ];

    if (arch == "armv7") {
        arch_sources = armv7_sources;

    } else if (arch == "armv6") {
        arch_sources = armv6_sources;

    } else if (arch == "x86") {
        arch_sources = x86_sources;
    }

    if (build_arch == "armv7") {
        build_arch_sources = armv7_sources;

    } else if (build_arch == "armv6") {
        build_arch_sources = armv6_sources;

    } else if (build_arch == "x86") {
        build_arch_sources = x86_sources;
    }

    lib = {
        "label": "mm",
        "inputs": base_sources + arch_sources,
    };

    boot_lib = {
        "label": "mmboot",
        "inputs": boot_sources
    };

    build_lib = {
        "label": "build_mm",
        "output": "mm",
        "inputs": base_sources + build_arch_sources,
        "build": TRUE,
        "prefix": "build"
    };

    entries = static_library(lib);
    entries += static_library(boot_lib);
    entries += static_library(build_lib);
    return entries;
}

return build();
