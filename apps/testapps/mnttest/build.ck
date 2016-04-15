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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "mnttest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
