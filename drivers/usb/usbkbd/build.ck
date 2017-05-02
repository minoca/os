/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    USB Keyboard

Abstract:

    This module implements the USB Keyboard class interface driver.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "usbkbd";
    var sources;

    sources = [
        "keycode.c",
        "usbkbd.c"
    ];

    dynlibs = [
        "drivers/usb/usbcore:usbcore",
        "drivers/input/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

