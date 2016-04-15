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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "ktest",
        "inputs": sources + dynlibs,
        "orderonly": [":ktestdrv"],
        "sources_config": sources_config
    };

    ktest_driver = {
        "label": "ktestdrv",
        "inputs": driver_sources + driver_dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    entries += driver(ktest_driver);
    return entries;
}

return build();
