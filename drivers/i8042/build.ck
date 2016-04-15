/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    i8042 Keyboard

Abstract:

    This module implements a keyboard and mouse driver for the Intel 8042
    keyboard controller.

Author:

    Evan Green 20-Dec-2012

Environment:

    Kernel

--*/

function build() {
    name = "i8042";
    sources = [
        "i8042.c",
        "scancode.c"
    ];

    dynlibs = [
        "//drivers/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
