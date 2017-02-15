/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    RK808

Abstract:

    This module is the driver for the RK808 Power Management IC used in
    platforms like the ASUS C201 Chromebook (Veyron Speedy).

Author:

    Evan Green 4-Apr-2016

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "rk808";
    var sources;

    sources = [
        "rk808.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

