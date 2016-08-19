/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    mount

Abstract:

    This executable implements the mount application. It is used to mount
    block devices, directories, devices, and files onto other directories
    or files.

Author:

    Chris Stevens 30-Jul-2013

Environment:

    User

--*/

function build() {
    sources = [
        "mount.c"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    includes = [
        "$//apps/libc/include"
    ];

    app = {
        "label": "mount",
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
