/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    usbrelay

Abstract:

    This executable implements the usbrelay application, which is a
    simple application that connects to and communicates with the USB
    relay board from One Ring Road.

Author:

    Evan Green 26-Jan-2015

Environment:

    Kernel

--*/

function build() {
    name = "usbrelay";
    sources = [
        "usbrelay.c"
    ];

    includes = [
        "$//apps/include"
    ];

    dynlibs = [
        "//apps/osbase:libminocaos"
    ];

    app = {
        "label": name,
        "inputs": sources + dynlibs,
        "includes": includes
    };

    entries = application(app);
    return entries;
}

return build();
