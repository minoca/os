/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB Mass Storage

Abstract:

    This module implements the USB Mass Storage class interface driver.

Author:

    Evan Green 28-Jan-2013

Environment:

    Kernel

--*/

function build() {
    name = "usbmass";
    sources = [
        "usbmass.c"
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
