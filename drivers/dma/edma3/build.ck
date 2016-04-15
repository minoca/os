/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    EDMA3

Abstract:

    This module implements support for the TI EDMA 3 controller.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

function build() {
    name = "edma3";
    sources = [
        "edma3.c"
    ];

    dynlibs = [
        "//drivers/dma/core:dma"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
