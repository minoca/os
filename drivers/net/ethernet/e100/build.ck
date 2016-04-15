/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Intel e100

Abstract:

    This module implements the Intel E100 integrated LAN controller
    driver (sometimes known as eepro100).

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

function build() {
    name = "e100";
    sources = [
        "e100.c",
        "e100hw.c"
    ];

    dynlibs = [
        "//drivers/net/netcore:netcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
