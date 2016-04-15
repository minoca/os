/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Kernel Executive

Abstract:

    This library contains the Kernel Executive, which is provides high
    level kernel services such as timekeeping, work queues, DPCs,
    scheduling, and synchronization primitives.

Author:

    Evan Green 5-Aug-2012

Environment:

    Kernel

--*/

function build() {
    sources = [
        "crash.c",
        "crashdmp.c",
        "dpc.c",
        "event.c",
        "info.c",
        "init.c",
        "ipi.c",
        "lock.c",
        "random.c",
        "reset.c",
        "runlevel.c",
        "sched.c",
        "syscall.c",
        "sysclock.c",
        "sysres.c",
        "timer.c",
        "timezone.c",
        "version.c",
        "video.c",
        "workitem.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/archinit.c",
            "armv7/ctxswap.S",
            "armv7/ctxswapc.c",
            "armv7/dispatch.c",
            "armv7/proc.c"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        arch_sources = [
            "x86/archinit.c",
            "x86/ctxswap.S",
            "x86/ctxswapc.c",
            "x86/dispatch.c",
            "x86/proc.c"
        ];
    }

    lib = {
        "label": "ke",
        "inputs": sources + arch_sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
