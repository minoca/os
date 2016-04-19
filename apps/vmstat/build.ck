/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    vmstat

Abstract:

    This executable implements the vmstat application, which prints current
    information about kernel memory usage.

Author:

    Evan Green 5-Mar-2015

Environment:

    User

--*/

function build() {
    sources = [
        "vmstat.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/include",
        "$//apps/include/libc"
    ];

    app = {
        "label": "vmstat",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
