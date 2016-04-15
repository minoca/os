/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

function build() {
    name = "net80211";
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
