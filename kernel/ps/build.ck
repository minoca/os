/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import kernelLibrary, mconfig;

function build() {
    var arch = mconfig.arch;
    var archSources;
    var baseSources;
    var entries;
    var lib;

    baseSources = [
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
        "utimer.c",
        "uts.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        archSources = [
            "armv7/psarch.c"
        ];

    } else if (arch == "x86") {
        archSources = [
            "x86/psarch.c"
        ];

    } else if (arch == "x64") {
        archSources = [
            "x64/psarch.c"
        ];
    }

    lib = {
        "label": "ps",
        "inputs": baseSources + archSources,
    };

    entries = kernelLibrary(lib);
    return entries;
}

