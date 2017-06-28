/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Platform

Abstract:

    This directory builds platform support drivers.

Author:

    Evan Green 4-Dec-2014

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var platformDrivers;

    if ((arch == "armv7") || (arch == "armv6")) {
        platformDrivers = [
            "drivers/plat/goec:goec",
            "drivers/plat/rockchip/rk808:rk808",
            "drivers/plat/ti/am3soc:am3soc",
            "drivers/plat/ti/tps65217:tps65217"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        platformDrivers = [
            "drivers/plat/quark/qrkhostb:qrkhostb"
        ];
    }

    entries = group("platform_drivers", platformDrivers);
    return entries;
}

