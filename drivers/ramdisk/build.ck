/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    RAM Disk

Abstract:

    This module implements a simple RAM disk driver.

Author:

    Evan Green 17-Oct-2012

Environment:

    Kernel

--*/

function build() {
    name = "ramdisk";
    sources = [
        "ramdisk.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
