/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    BCM27xx GPIO

Abstract:

    This module implements General Purpose I/O support for the Broadcom
    27xx SoCs.

Author:

    Chris Stevens 10-May-2016

Environment:

    Kernel

--*/

function build() {
    name = "bc27gpio";
    sources = [
        "bc27gpio.c"
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
