/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Process/Thread Library

Abstract:

    This library contains the process and thread library. It maintains the
    lifecycle of threads (units of execution) and processes (collections of
    threads in a shared address space).

Author:

    Evan Green 6-Aug-2012

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "env.c",
        "info.c",
        "init.c",
        "perm.c",
        "pgroups.c",
        "process.c",
        "psimag.c",
        "signals.c",
        "thread.c",
        "usrlock.c",
        "utimer.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/psarch.c"
        ];

    } else if (arch == "x86") {
        arch_sources = [
            "x86/psarch.c"
        ];
    }

    lib = {
        "label": "ps",
        "inputs": base_sources + arch_sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
