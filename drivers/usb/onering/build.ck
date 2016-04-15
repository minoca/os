/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    One Ring

Abstract:

    This module implements the USB LED and USB Relay devices from One Ring
    Road.

Author:

    Evan Green 15-Jul-2014

Environment:

    Kernel

--*/

function build() {
    name = "onering";
    sources = [
        "onering.c"
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
