/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    EHCI

Abstract:

    This module implements the EHCI USB 2.0 Host Controller Driver.

Author:

    Evan Green 18-Mar-2013

Environment:

    Kernel

--*/

function build() {
    name = "ehci";
    sources = [
        "ehci.c",
        "ehcihc.c"
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
