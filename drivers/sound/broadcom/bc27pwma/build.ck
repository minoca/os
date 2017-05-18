/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Broadcom 2709 PWM Audio

Abstract:

    This module implements Broadcom 2709 PWM Audio support.

Author:

    Chris Stevens 2-May-2017

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "bc27pwma";
    var sources;

    sources = [
        "pwma.c",
    ];

    dynlibs = [
        "drivers/sound/core:sound"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

