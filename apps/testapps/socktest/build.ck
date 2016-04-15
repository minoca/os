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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "socktest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
