/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    ATA

Abstract:

    This module implements the driver for the AT Attachment (ATA) controller.

Author:

    Evan Green 5-Jun-2014

Environment:

    Kernel

--*/

function build() {
    name = "ata";
    sources = [
        "ata.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
