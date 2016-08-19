/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Mmap Test

Abstract:

    This executable implements the memory map test application.

Author:

    Chris Stevens 10-Mar-2014

Environment:

    User

--*/

function build() {
    sources = [
        "mmaptest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "mmaptest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
