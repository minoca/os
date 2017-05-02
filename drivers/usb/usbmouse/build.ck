/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    USB Mouse

Abstract:

    This module implements the USB Mouse class interface driver.

Author:

    Evan Green 14-Mar-2017

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "usbmouse";
    var sources;

    sources = [
        "usbmouse.c"
    ];

    dynlibs = [
        "drivers/usb/usbcore:usbcore",
        "drivers/usb/usbhid:usbhid",
        "drivers/input/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

