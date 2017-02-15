/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Null

Abstract:

    This module implements a null driver that simply passes all IRPs down.
    It serves as an excellent skeleton for starting a new driver, and also
    provides a handy stub driver for instances where all functionality is
    provided by bus drivers or other mechanisms.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "null";
    var sources;

    sources = [
        "null.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

