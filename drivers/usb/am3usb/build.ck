/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    TI AM33xx USB

Abstract:

    This module implements the USB 2.0 OTG controller on TI AM33xx SoCs.

Author:

    Evan Green 11-Sep-2015

Environment:

    Kernel

--*/

function build() {
    name = "am3usb";
    sources = [
        "am3usb.c",
        "am3usbhw.c",
        "cppi.c",
        "musb.c"
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
