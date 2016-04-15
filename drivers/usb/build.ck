/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    USB

Abstract:

    This directory contains Universal Serial Bus (USB) related drivers,
    including the USB core support library, host controller implementations,
    generic device class drivers, and specific USB device drivers.

Author:

    Evan Green 13-Jan-2013

Environment:

    Kernel

--*/

function build() {
    usb_drivers = [
        "//drivers/usb/ehci:ehci",
        "//drivers/usb/onering:onering",
        "//drivers/usb/onering/usbrelay:usbrelay",
        "//drivers/usb/usbcomp:usbcomp",
        "//drivers/usb/usbhub:usbhub",
        "//drivers/usb/usbkbd:usbkbd",
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        usb_drivers = [
            "//drivers/usb/am3usb:am3usb",
            "//drivers/usb/dwhci:dwhci"
        ];

    } else if (arch == "x86") {
        usb_drivers = [
            "//drivers/usb/uhci:uhci"
        ];
    }

    entries = group("usb_drivers", usb_drivers);
    return entries;
}

return build();
