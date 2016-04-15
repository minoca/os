/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    SMSC 95xx

Abstract:

    This module implements support for the SMSC95xx family of USB ethernet
    controllers.

Author:

    Evan Green 7-Nov-2013

Environment:

    Kernel

--*/

function build() {
    name = "smsc95xx";
    sources = [
        "smsc95.c",
        "smsc95hw.c"
    ];

    dynlibs = [
        "//drivers/net/netcore:netcore",
        "//drivers/usb/usbcore:usbcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
