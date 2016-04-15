/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    DMA

Abstract:

    This directory builds system DMA drivers.

Author:

    Evan Green 1-Feb-2016

Environment:

    Kernel

--*/

function build() {

    assert(((arch == "armv7") || (arch == "armv6")), "Unexpected architecture");

    dma_drivers = [
        "//drivers/dma/bcm2709:dmab2709",
        "//drivers/dma/edma3:edma3",
    ];

    entries = group("dma_drivers", dma_drivers);
    return entries;
}

return build();
