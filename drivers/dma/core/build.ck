/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    DMA Core

Abstract:

    This module implements the DMA core support library. It provides
    generic support infrastructure for all DMA controllers.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

function build() {
    name = "dma";
    sources = [
        "dma.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
