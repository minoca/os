/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Performance Benchmark Test

Abstract:

    This executable implements the performance benchmark test application.

Author:

    Evan Green 27-Apr-2015

Environment:

    User

--*/

from menv import application, sharedLibrary;

function build() {
    var app;
    var dynlibs;
    var entries;
    var includes;
    var libSources;
    var perfLib;
    var sources;

    sources = [
        "copy.c",
        "create.c",
        "dlopen.c",
        "dup.c",
        "getppid.c",
        "exec.c",
        "fork.c",
        "malloc.c",
        "mmap.c",
        "mutex.c",
        "open.c",
        "perfsup.c",
        "perftest.c",
        "pipeio.c",
        "pthread.c",
        "read.c",
        "rename.c",
        "stat.c",
        "write.c"
    ];

    libSources = [
        "perflib/perflib.c"
    ];

    dynlibs = [
        "apps/osbase:libminocaos"
    ];

    includes = [
        "$S/apps/libc/include"
    ];

    app = {
        "label": "perftest",
        "inputs": sources + dynlibs,
        "orderonly": [":perflib"],
        "includes": includes
    };

    perfLib = {
        "label": "perflib",
        "inputs": libSources,
        "includes": includes
    };

    entries = application(app);
    entries += sharedLibrary(perfLib);
    return entries;
}

