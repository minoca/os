/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
        "$//apps/libc/include"
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
