/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    TPS65217

Abstract:

    This module is the driver for the TPS65217 Power Management IC used in
    platforms like the BeagleBone Black.

Author:

    Evan Green 8-Sep-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "tps65217";
    var sources;

    sources = [
        "tps65217.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

