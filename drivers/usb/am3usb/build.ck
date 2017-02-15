/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    TI AM33xx USB

Abstract:

    This module implements the USB 2.0 OTG controller on TI AM33xx SoCs.

Author:

    Evan Green 11-Sep-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "am3usb";
    var sources;

    sources = [
        "am3usb.c",
        "am3usbhw.c",
        "cppi.c",
        "musb.c"
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

