/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    profile

Abstract:

    This executable implements the profile application. It is used to
    enable and disable the Minoca System Profiler.

Author:

    Chris Stevens 18-Jan-2015

Environment:

    User

--*/

function build() {
    sources = [
        "profile.c"
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
        "label": "profile",
        "inputs": sources + dynlibs,
        "sources_config": sources_config
    };

    entries = application(app);
    return entries;
}

return build();
