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

function build() {
    name = "pl050";
    sources = [
        "pl050.c",
        "//drivers/i8042:scancode.o"
    ];

    dynlibs = [
        "//drivers/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
