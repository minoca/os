/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    DWHCI

Abstract:

    This module implements a DesignWare High-Speed USB 2.0 On-The-Go
    (HS OTG) Host Controller Driver.

Author:

    Chris Stevens 27-Mar-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "dwhci";
    var sources;

    sources = [
        "dwhci.c",
        "dwhcihc.c",
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

