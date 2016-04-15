/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Serial 16550

Abstract:

    This module implements support for 16550-like UARTs.

Author:

    Evan Green 21-Nov-2014

Environment:

    Kernel

--*/

function build() {
    name = "ser16550";
    sources = [
        "ser16550.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
