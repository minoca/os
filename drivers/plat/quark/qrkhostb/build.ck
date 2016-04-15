/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Quark Host Bridge

Abstract:

    This module implements the Quark Host Bridge driver, which includes
    support for features like IMRs (Isolated Memory Regions).

Author:

    Evan Green 4-Dec-2014

Environment:

    Kernel

--*/

function build() {
    name = "qrkhostb";
    sources = [
        "qrkhostb.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
