/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    RockChip RK3xxx I2C Driver

Abstract:

    This module implements the I2C controller driver for the RockChip
    RK3xxx SoC.

Author:

    Evan Green 1-Apr-2016

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "rk3i2c";
    var sources;

    sources = [
        "rk3i2c.c"
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

