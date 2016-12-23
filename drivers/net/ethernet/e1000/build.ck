/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Intel e1000

Abstract:

    This module implements the Intel E1000 integrated LAN controller
    driver.

Author:

    Evan Green 8-Nov-2016

Environment:

    Kernel

--*/

function build() {
    name = "e1000";
    sources = [
        "e1000.c",
        "e1000hw.c"
    ];

    dynlibs = [
        "//drivers/net/netcore:netcore"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

return build();
