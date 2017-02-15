/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Special

Abstract:

    This module implements a special file driver that implements devices
    like null, zero, and full.

Author:

    Evan Green 23-Sep-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "special";
    var sources;

    sources = [
        "special.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

