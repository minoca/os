/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    TI AM335x I2C Driver

Abstract:

    This module implements the I2C controller driver for the TI AM335x SoC.

Author:

    Evan Green 7-Sep-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "am3i2c";
    var sources;

    sources = [
        "am3i2c.c"
    ];

    dynlibs = [
        "drivers/spb/core:spb"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

