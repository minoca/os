/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Debug Test

Abstract:

    This executable implements the debug API test application.

Author:

    Evan Green 15-May-2013

Environment:

    User

--*/

function build() {
    sources = [
        "dbgtest.c"
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
        "label": "dbgtest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
