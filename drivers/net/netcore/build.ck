/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "netcore";
    var sources;

    sources = [
        "addr.c",
        "buf.c",
        "ethernet.c",
        "ipv4/arp.c",
        "ipv4/dhcp.c",
        "ipv4/igmp.c",
        "ipv4/ip4.c",
        "ipv6/icmp6.c",
        "ipv6/ip6.c",
        "ipv6/mld.c",
        "ipv6/ndp.c",
        "mcast.c",
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

