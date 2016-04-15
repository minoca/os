/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    SD

Abstract:

    This module implements the Secure Digital host controller driver. It
    can be used as a standalone driver or imported as a support library.

Author:

    Evan Green 27-Feb-2014

Environment:

    Kernel

--*/

function build() {
    name = "sd";
    sources = [
        "sd.c",
        "sdlib.c",
        "sdstd.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
