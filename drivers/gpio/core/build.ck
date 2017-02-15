/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    GPIO Core

Abstract:

    This module implements the GPIO core support library. It provides
    generic support infrastructure for all GPIO drivers.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "gpio";
    var sources;

    sources = [
        "gpio.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

