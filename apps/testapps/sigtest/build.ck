/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Signal Test

Abstract:

    This executable implements the signal test application.

Author:

    Evan Green 6-May-2013

Environment:

    User

--*/

function build() {
    sources = [
        "sigtest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/include",
        "$//apps/include/libc"
    ];

    app = {
        "label": "sigtest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
