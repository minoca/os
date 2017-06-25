/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import kernelLibrary, addConfig, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var entries;
    var lib;
    var sources;

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
        "version.c",
        "video.c",
        "workitem.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        archSources = [
            "armv7/archinit.c",
            "armv7/ctxswap.S",
            "armv7/ctxswapc.c",
            "armv7/dispatch.c",
            "armv7/proc.c"
        ];

    } else if (arch == "x86") {
        archSources = [
            "x86/archinit.c",
            "x86/ctxswap.S",
            "x86/ctxswapc.c",
            "x86/dispatch.c",
            "x86/pcexcept.c",
            "x86/proc.c"
        ];

    } else if (arch == "x64") {
        archSources = [
            "x64/ctxswap.S",
            "x64/ctxswapc.c",
            "x64/dispatch.c",
            "x86/archinit.c",
            "x86/pcexcept.c",
            "x86/proc.c"
        ];
    }

    lib = {
        "label": "ke",
        "inputs": sources + archSources,
    };

    entries = kernelLibrary(lib);

    //
    // Add the include and dependency for version.c.
    //

    for (entry in entries) {
        if (entry["output"] == "version.o") {
            addConfig(entry, "CPPFLAGS", "-I$O/kernel");
            entry["implicit"] = ["kernel:version.h"];
            break;
        }
    }

    return entries;
}

