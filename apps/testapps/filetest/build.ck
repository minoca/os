/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    File Test

Abstract:

    This executable implements the file test application.

Author:

    Evan Green 27-Sep-2013

Environment:

    User

--*/

function build() {
    sources = [
        "filetest.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "filetest",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
