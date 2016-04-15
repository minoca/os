/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Special

Abstract:

    This module implements a special file driver that implements devices
    like null, zero, and full.

Author:

    Evan Green 23-Sep-2013

Environment:

    Kernel

--*/

function build() {
    name = "special";
    sources = [
        "special.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
