/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

Module Name:

    Broadcom 27xx I2C Driver

Abstract:

    This module implements the I2C controller driver for the Broadcom 27xx SoC.

Author:

    Chris Stevens 18-Jan-2017

Environment:

    Kernel

--*/

function build() {
    name = "bcm27i2c";
    sources = [
        "bcm27i2c.c"
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
