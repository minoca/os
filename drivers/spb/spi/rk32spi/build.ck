/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    RK32 SPI Controller

Abstract:

    This directory contains the controller support for SPI used on the
    RockChip RK3288.

Author:

    Evan Green 14-Aug-2015

Environment:

    Kernel

--*/

function build() {
    name = "rk32spi";
    sources = [
        "rk32spi.c"
    ];

    dynlibs = [
        "//drivers/spb/core:spb"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
