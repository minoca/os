/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Simple Peripheral Bus

Abstract:

    This directory contains drivers related to Simple Peripheral Busses.

Author:

    Evan Green 14-Aug-2015

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var spbDrivers;

    if ((arch != "armv7") && (arch != "armv6")) {
        Core.raise(ValueError("Unexpected architecture"));
    }

    spbDrivers = [
        "drivers/spb/i2c/am3i2c:am3i2c",
        "drivers/spb/i2c/bcm27i2c:bcm27i2c",
        "drivers/spb/i2c/rk3i2c:rk3i2c",
        "drivers/spb/spi/rk32spi:rk32spi"
    ];

    entries = group("spb_drivers", spbDrivers);
    return entries;
}

