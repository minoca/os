/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    RK32 SPI Controller

Abstract:

    This directory contains the controller support for SPI used on the
    RockChip RK3288.

Author:

    Evan Green 14-Aug-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "rk32spi";
    var sources;

    sources = [
        "rk32spi.c"
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

