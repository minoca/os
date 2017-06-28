/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SD

Abstract:

    This directory is responsible for building Secure Digital and MultiMedia
    Card host controller drivers.

Author:

    Evan Green 16-Mar-2014

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var sdDrivers;

    if ((arch == "armv7") || (arch == "armv6")) {
        sdDrivers = [
            "drivers/sd/bcm2709:sdbm2709",
            "drivers/sd/omap4:sdomap4",
            "drivers/sd/rk32xx:sdrk32xx",
            "drivers/plat/ti/tps65217:tps65217"
        ];

    } else if ((arch == "x86") || (arch == "x64")) {
        sdDrivers = [
            "drivers/sd/core:sd"
        ];
    }

    entries = group("sd_drivers", sdDrivers);
    return entries;
}

