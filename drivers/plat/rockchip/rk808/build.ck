/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    RK808

Abstract:

    This module is the driver for the RK808 Power Management IC used in
    platforms like the ASUS C201 Chromebook (Veyron Speedy).

Author:

    Evan Green 4-Apr-2016

Environment:

    Kernel

--*/

function build() {
    name = "rk808";
    sources = [
        "rk808.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
