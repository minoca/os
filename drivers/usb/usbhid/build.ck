/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    USB HID

Abstract:

    This module implements the USB HID report support library.

Author:

    Evan Green 15-Mar-2017

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "usbhid";
    var sources;

    sources = [
        "hiddrv.c",
        "usbhid.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

