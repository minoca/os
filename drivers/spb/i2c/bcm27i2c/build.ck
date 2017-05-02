/*++

Copyright (c) 2017 Minoca Corp. All Rights Reserved

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Broadcom 27xx I2C Driver

Abstract:

    This module implements the I2C controller driver for the Broadcom 27xx SoC.

Author:

    Chris Stevens 18-Jan-2017

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name;
    var sources;

    name = "bcm27i2c";
    sources = [
        "bcm27i2c.c"
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

