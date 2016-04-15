/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    BCM2709 DMA

Abstract:

    This module implements support for the Broadcom 2709 DMA controller.

Author:

    Chris Stevens 12-Feb-2016

Environment:

    Kernel

--*/

function build() {
    name = "dmab2709";
    sources = [
        "dmab2709.c"
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
