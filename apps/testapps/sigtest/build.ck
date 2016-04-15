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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "sigtest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
