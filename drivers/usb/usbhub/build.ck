/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB Hub

Abstract:

    This module implements the USB Hub class driver.

Author:

    Evan Green 16-Jan-2013

Environment:

    Kernel

--*/

function build() {
    name = "usbhub";
    sources = [
        "usbhub.c"
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
