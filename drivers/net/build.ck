/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Networking

Abstract:

    This directory contains networking-related drivers, including the
    networking core driver and support for many specific NICs.

Author:

    Evan Green 4-Apr-2013

Environment:

    Kernel

--*/

function build() {
    ethernet_drivers = [
        "//drivers/net/ethernet/smsc95xx:smsc95xx",
    ];

    wireless_drivers = [
        "//drivers/net/wireless/rtlw81xx:rtlw81xx",
    ];

    if ((arch == "armv7") || (arch == "armv6")) {
        ethernet_drivers += [
            "//drivers/net/ethernet/smsc91c1:smsc91c1",
            "//drivers/net/ethernet/am3eth:am3eth"
        ];

    } else if (arch == "x86") {
        ethernet_drivers += [
            "//drivers/net/ethernet/atl1c:atl1c",
            "//drivers/net/ethernet/dwceth:dwceth",
            "//drivers/net/ethernet/e100:e100",
            "//drivers/net/ethernet/e1000:e1000",
            "//drivers/net/ethernet/pcnet32:pcnet32",
            "//drivers/net/ethernet/rtl81xx:rtl81xx",
        ];
    }

    net_drivers = ethernet_drivers + wireless_drivers;
    entries = group("net_drivers", net_drivers);
    return entries;
}

return build();
