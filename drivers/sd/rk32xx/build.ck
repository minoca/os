/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    SD Rockchip 32xx

Abstract:

    This module implements the Rockchip 32xx SD controller driver.

Author:

    Chris Stevens 29-Jul-2015

Environment:

    Kernel

--*/

function build() {
    name = "sdrk32xx";
    sources = [
        "sdrk32.c"
    ];

    dynlibs = [
        "//drivers/sd/core:sd"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
