/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SMSC 91C111

Abstract:

    This module implements support for the SMSC91C111 ethernet controller.

Author:

    Chris Stevens 16-Apr-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "smsc91c1";
    var sources;

    sources = [
        "sm91c1.c",
        "sm91c1hw.c"
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

