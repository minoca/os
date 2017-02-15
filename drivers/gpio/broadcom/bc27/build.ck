/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    BCM27xx GPIO

Abstract:

    This module implements General Purpose I/O support for the Broadcom
    27xx SoCs.

Author:

    Chris Stevens 10-May-2016

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "bc27gpio";
    var sources;

    sources = [
        "bc27gpio.c"
    ];

    dynlibs = [
        "drivers/gpio/core:gpio"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

