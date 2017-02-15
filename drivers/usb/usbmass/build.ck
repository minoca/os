/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    USB Mass Storage

Abstract:

    This module implements the USB Mass Storage class interface driver.

Author:

    Evan Green 28-Jan-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "usbmass";
    var sources;

    sources = [
        "usbmass.c"
    ];

    dynlibs = [
        "drivers/usb/usbcore:usbcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

