/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    RTL 81xx

Abstract:

    This module implements support for the Realtek RTL81xx family of
    ethernet controllers.

Author:

    Chris Stevens 20-June-2014

Environment:

    Kernel

--*/

function build() {
    name = "rtl81xx";
    sources = [
        "rtl81.c",
        "rtl81hw.c"
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
