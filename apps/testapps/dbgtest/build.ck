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
        "$//apps/libc/include"
    ];

    app = {
        "label": "dbgtest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
