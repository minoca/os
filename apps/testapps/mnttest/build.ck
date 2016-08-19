/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Mount Test

Abstract:

    This executable implements the mount test application.

Author:

    Chris Stevens 25-Nov-2013

Environment:

    User

--*/

function build() {
    sources = [
        "mnttest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "mnttest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
