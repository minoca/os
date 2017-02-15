/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SMSC 95xx

Abstract:

    This module implements support for the SMSC95xx family of USB ethernet
    controllers.

Author:

    Evan Green 7-Nov-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "smsc95xx";
    var sources;

    sources = [
        "smsc95.c",
        "smsc95hw.c"
    ];

    dynlibs = [
        "drivers/net/netcore:netcore",
        "drivers/usb/usbcore:usbcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

