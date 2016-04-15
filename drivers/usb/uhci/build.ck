/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    UHCI

Abstract:

    This module implements the UHCI USB 1.1 Host Controller Driver.

Author:

    Evan Green 13-Jan-2013

Environment:

    Kernel

--*/

function build() {
    name = "uhci";
    sources = [
        "uhci.c",
        "uhcihc.c"
    ];

    dynlibs = [
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
