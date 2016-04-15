/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    SMSC 91C111

Abstract:

    This module implements support for the SMSC91C111 ethernet controller.

Author:

    Chris Stevens 16-Apr-2014

Environment:

    Kernel

--*/

function build() {
    name = "smsc91c1";
    sources = [
        "sm91c1.c",
        "sm91c1hw.c"
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
