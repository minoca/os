/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB Core

Abstract:

    This module implements the USB core. It manages host controllers,
    enumerates devices, manages transfers, and generally provides the high
    level glue needed to make USB work.

Author:

    Evan Green 15-Jan-2013

Environment:

    Kernel

--*/

function build() {
    name = "usbcore";
    sources = [
        "enum.c",
        "hub.c",
        "usbcore.c",
        "usbhost.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
