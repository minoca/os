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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "filetest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
