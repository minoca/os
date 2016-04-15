/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    DesignWare Ethernet

Abstract:

    This module implements the Synopsys DesignWare Ethernet network driver.

Author:

    Evan Green 5-Dec-2014

Environment:

    Kernel

--*/

function build() {
    name = "dwceth";
    sources = [
        "dwceth.c",
        "dwcethhw.c"
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
