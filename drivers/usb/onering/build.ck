/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "onering";
    var sources;

    sources = [
        "onering.c"
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

