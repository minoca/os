/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
        "//drivers/usb/usbmass:usbmass"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        usb_drivers += [
            "//drivers/usb/am3usb:am3usb",
            "//drivers/usb/dwhci:dwhci"
        ];

    } else if (arch == "x86") {
        usb_drivers += [
            "//drivers/usb/uhci:uhci"
        ];
    }

    entries = group("usb_drivers", usb_drivers);
    return entries;
}

return build();
