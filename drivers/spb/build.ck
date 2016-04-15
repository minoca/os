/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    Simple Peripheral Bus

Abstract:

    This directory contains drivers related to Simple Peripheral Busses.

Author:

    Evan Green 14-Aug-2015

Environment:

    Kernel

--*/

function build() {

    assert(((arch == "armv7") || (arch == "armv6")), "Unexpected architecture");

    spb_drivers = [
        "//drivers/spb/i2c/am3i2c:am3i2c",
        "//drivers/spb/i2c/rk3i2c:rk3i2c",
        "//drivers/spb/spi/rk32spi:rk32spi"
    ];

    entries = group("spb_drivers", spb_drivers);
    return entries;
}

return build();
