/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    TI AM335x I2C Driver

Abstract:

    This module implements the I2C controller driver for the TI AM335x SoC.

Author:

    Evan Green 7-Sep-2015

Environment:

    Kernel

--*/

function build() {
    name = "am3i2c";
    sources = [
        "am3i2c.c"
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
