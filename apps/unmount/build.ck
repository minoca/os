/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    umount

Abstract:

    This executable implements the unmount application.

Author:

    Chris Stevens 30-Jul-2013

Environment:

    User

--*/

function build() {
    sources = [
        "unmount.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "umount",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
