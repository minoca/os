/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i8042 Keyboard

Abstract:

    This module implements a keyboard and mouse driver for the Intel 8042
    keyboard controller.

Author:

    Evan Green 20-Dec-2012

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "i8042";
    var sources;

    sources = [
        "i8042.c",
        "scancode.c"
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

