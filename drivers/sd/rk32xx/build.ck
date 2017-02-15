/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SD Rockchip 32xx

Abstract:

    This module implements the Rockchip 32xx SD controller driver.

Author:

    Chris Stevens 29-Jul-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "sdrk32xx";
    var sources;

    sources = [
        "sdrk32.c"
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

