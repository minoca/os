/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    PL-050 Keyboard

Abstract:

    This module implements a keyboard and mouse driver for the ARM
    PrimeCell PL050 keyboard and mouse controller.

Author:

    Evan Green 22-Sep-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "pl050";
    var sources;

    sources = [
        "pl050.c",
        "drivers/input/i8042:scancode.o"
    ];

    dynlibs = [
        "drivers/input/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

