/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SD BCM2709

Abstract:

    This module implements the BCM2709 SD controller driver.

Author:

    Chris Stevens 10-Dec-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "sdbm2709";
    var sources;

    sources = [
        "emmc.c",
        "sdbm2709.c"
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

