/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Kernel Test

Abstract:

    This executable implements the kernel test application, which loads a
    driver, executes kernel mode stress tests, and reports the results back
    to user mode.

Author:

    Evan Green 5-Nov-2013

Environment:

    User

--*/

function build() {
    sources = [
        "ktest.c"
    ];

    driver_sources = [
        "driver/ktestdrv.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    driver_dynlibs = [
        "//kernel:kernel"
    ];

    includes = [
        "$//apps/include",
        "$//apps/include/libc"
    ];

    app = {
        "label": "ktest",
        "inputs": sources + dynlibs,
        "orderonly": [":ktestdrv"],
        "includes": includes
    };

    ktest_driver = {
        "label": "ktestdrv",
        "inputs": driver_sources + driver_dynlibs,
        "includes": includes
    };

    entries = application(app);
    entries += driver(ktest_driver);
    return entries;
}

return build();
