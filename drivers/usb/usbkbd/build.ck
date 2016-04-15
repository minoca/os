/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB Keyboard

Abstract:

    This module implements the USB Keyboard class interface driver.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

function build() {
    name = "usbkbd";
    sources = [
        "keycode.c",
        "usbkbd.c"
    ];

    dynlibs = [
        "//drivers/usb/usbcore:usbcore",
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
