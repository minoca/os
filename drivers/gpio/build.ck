/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    GPIO

Abstract:

    This directory contains drivers related to General Purpose Input/Output.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

function build() {

    assert(((arch == "armv7") || (arch == "armv6")), "Unexpected architecture");

    gpio_drivers = [
        "//drivers/gpio/broadcom/bc27:bc27gpio",
        "//drivers/gpio/rockchip/rk32:rk32gpio",
        "//drivers/gpio/ti/omap4:om4gpio",
    ];

    entries = group("gpio_drivers", gpio_drivers);
    return entries;
}

return build();
