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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "vmstat",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
