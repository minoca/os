/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    Null

Abstract:

    This module implements a null driver that simply passes all IRPs down.
    It serves as an excellent skeleton for starting a new driver, and also
    provides a handy stub driver for instances where all functionality is
    provided by bus drivers or other mechanisms.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

function build() {
    name = "null";
    sources = [
        "null.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
