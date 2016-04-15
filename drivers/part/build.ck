/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    Partition

Abstract:

    This module implements the partition support driver.

Author:

    Evan Green 30-Jan-2014

Environment:

    Kernel

--*/

function build() {
    name = "part";
    sources = [
        "part.c"
    ];

    libs = [
        "//lib/partlib:partlib"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

return build();
