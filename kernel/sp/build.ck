/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    System Profiler

Abstract:

    This library contains the System Profiler, which lends insight into the
    real-time resource usage of the system.

Author:

    Chris Stevens 1-Jul-2013

Environment:

    Kernel

--*/

function build() {
    base_sources = [
        "info.c",
        "profiler.c"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        arch_sources = [
            "armv7/archprof.c"
        ];

    } else if (arch == "x86") {
        arch_sources = [
            "x86/archprof.c"
        ];
    }

    lib = {
        "label": "sp",
        "inputs": base_sources + arch_sources,
    };

    entries = static_library(lib);
    return entries;
}

return build();
