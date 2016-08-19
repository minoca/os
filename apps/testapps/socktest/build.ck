/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Socket Test

Abstract:

    This executable implements the socket test application.

Author:

    Evan Green 6-May-2013

Environment:

    User

--*/

function build() {
    sources = [
        "socktest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "socktest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
