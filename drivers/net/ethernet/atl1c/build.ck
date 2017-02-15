/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ATL1c

Abstract:

    This module implements a NIC driver for the Atheros ATL1C family
    (formerly Attansic L1).

Author:

    Evan Green 18-Apr-2013

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "atl1c";
    var sources;

    sources = [
        "atl1c.c",
        "atl1chw.c"
    ];

    dynlibs = [
        "drivers/net/netcore:netcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

