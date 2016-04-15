/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB Composite

Abstract:

    This module implements the USB Composite device driver, which exposes
    child devices for each independent interface of a USB device.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

function build() {
    name = "usbcomp";
    sources = [
        "usbcomp.c"
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
