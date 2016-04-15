/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    TPS65217

Abstract:

    This module is the driver for the TPS65217 Power Management IC used in
    platforms like the BeagleBone Black.

Author:

    Evan Green 8-Sep-2015

Environment:

    Kernel

--*/

function build() {
    name = "tps65217";
    sources = [
        "tps65217.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
