/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Net 802.11 Core

Abstract:

    This module implements the IEEE 802.11 networking core. It manages
    802.11 wireless network traffic.

Author:

    Chris Stevens 22-Oct-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "net80211";
    var sources;

    sources = [
        "control.c",
        "crypto.c",
        "data.c",
        "eapol.c",
        "mgmt.c",
        "net80211.c",
        "netlink.c",
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

