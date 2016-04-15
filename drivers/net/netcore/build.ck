/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    Net Core

Abstract:

    This module implements the networking core. It manages network
    interfaces and provides support for core protocols like TCP, IP, IPv6,
    ARP, and others.

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

function build() {
    name = "netcore";
    sources = [
        "addr.c",
        "arp.c",
        "buf.c",
        "dhcp.c",
        "ethernet.c",
        "ip4.c",
        "netcore.c",
        "netlink/netlink.c",
        "netlink/genctrl.c",
        "netlink/generic.c",
        "raw.c",
        "tcp.c",
        "tcpcong.c",
        "udp.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

return build();
