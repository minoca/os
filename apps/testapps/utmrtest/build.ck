/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Timer Test

Abstract:

    This executable implements the user mode timer test application.

Author:

    Evan Green 11-Aug-2013

Environment:

    User

--*/

function build() {
    sources = [
        "utmrtest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "utmrtest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
