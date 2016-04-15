/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    RK32xx GPIO

Abstract:

    This module implements General Purpose I/O support for the RockChip
    RK32xx SoC.

Author:

    Evan Green 25-Aug-2015

Environment:

    Kernel

--*/

function build() {
    name = "rk32gpio";
    sources = [
        "rk32gpio.c"
    ];

    dynlibs = [
        "//drivers/gpio/core:gpio"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
