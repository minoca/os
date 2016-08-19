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
        "$//apps/libc/include"
    ];

    app = {
        "label": "profile",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
