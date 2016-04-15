/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    DWHCI

Abstract:

    This module implements a DesignWare High-Speed USB 2.0 On-The-Go
    (HS OTG) Host Controller Driver.

Author:

    Chris Stevens 27-Mar-2014

Environment:

    Kernel

--*/

function build() {
    name = "dwhci";
    sources = [
        "dwhci.c",
        "dwhcihc.c",
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
