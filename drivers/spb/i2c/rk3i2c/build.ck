/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    RockChip RK3xxx I2C Driver

Abstract:

    This module implements the I2C controller driver for the RockChip
    RK3xxx SoC.

Author:

    Evan Green 1-Apr-2016

Environment:

    Kernel

--*/

function build() {
    name = "rk3i2c";
    sources = [
        "rk3i2c.c"
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
