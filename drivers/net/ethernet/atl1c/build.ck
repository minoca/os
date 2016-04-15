/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    ATL1c

Abstract:

    This module implements a NIC driver for the Atheros ATL1C family
    (formerly Attansic L1).

Author:

    Evan Green 18-Apr-2013

Environment:

    Kernel

--*/

function build() {
    name = "atl1c";
    sources = [
        "atl1c.c",
        "atl1chw.c"
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
