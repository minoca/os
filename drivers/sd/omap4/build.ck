/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SD OMAP4

Abstract:

    This module implements the OMAP4 SD controller driver.

Author:

    Evan Green 16-Mar-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "sdomap4";
    var sources;

    sources = [
        "i2c.c",
        "pmic.c",
        "sdomap4.c"
    ];

    dynlibs = [
        "drivers/sd/core:sd"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

