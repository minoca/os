/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Path Test

Abstract:

    This executable implements the Path test application.

Author:

    Chris Stevens 17-Sep-2013

Environment:

    User

--*/

function build() {
    sources = [
        "pathtest.c"
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
        "label": "pathtest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
