/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    USB Core

Abstract:

    This module implements the USB core. It manages host controllers,
    enumerates devices, manages transfers, and generally provides the high
    level glue needed to make USB work.

Author:

    Evan Green 15-Jan-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "usbcore";
    var sources;

    sources = [
        "enum.c",
        "hub.c",
        "usbcore.c",
        "usbhost.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

