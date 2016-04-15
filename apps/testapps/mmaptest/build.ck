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
        "-I$///apps/include",
        "-I$///apps/include/libc"
    ];

    sources_config = {
        "CPPFLAGS": ["$CPPFLAGS"] + includes,
    };

    app = {
        "label": "mmaptest",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
