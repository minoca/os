/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    SD BCM2709

Abstract:

    This module implements the BCM2709 SD controller driver.

Author:

    Chris Stevens 10-Dec-2014

Environment:

    Kernel

--*/

function build() {
    name = "sdbm2709";
    sources = [
        "emmc.c",
        "sdbm2709.c"
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
