/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Device Removal Test

Abstract:

    This module implements a test driver that handles device removal.

Author:

    Chris Stevens 13-May-2013

Environment:

    Kernel

--*/

function build() {
    name = "devrem";
    sources = [
        "devrem.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
