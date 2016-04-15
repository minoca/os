/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    OMAP4 GPIO

Abstract:

    This module implements General Purpose I/O support for the TI OMAP4
    SoC in the kernel.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

function build() {
    name = "om4gpio";
    sources = [
        "om4gpio.c"
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
