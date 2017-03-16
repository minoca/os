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

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var usbDrivers;

    usbDrivers = [
        "drivers/usb/ehci:ehci",
        "drivers/usb/onering:onering",
        "drivers/usb/onering/usbrelay:usbrelay",
        "drivers/usb/usbcomp:usbcomp",
        "drivers/usb/usbhub:usbhub",
        "drivers/usb/usbhid:usbhid",
        "drivers/usb/usbkbd:usbkbd",
        "drivers/usb/usbmass:usbmass",
        "drivers/usb/usbmouse:usbmouse"
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        usbDrivers += [
            "drivers/usb/am3usb:am3usb",
            "drivers/usb/dwhci:dwhci"
        ];

    } else if (arch == "x86") {
        usbDrivers += [
            "drivers/usb/uhci:uhci"
        ];
    }

    entries = group("usb_drivers", usbDrivers);
    return entries;
}

