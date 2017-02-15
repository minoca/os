/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    GPIO

Abstract:

    This directory contains drivers related to General Purpose Input/Output.

Author:

    Evan Green 4-Aug-2015

Environment:

    Kernel

--*/

from menv import group, mconfig;

function build() {
    var arch = mconfig.arch;
    var entries;
    var gpioDrivers;

    if ((arch != "armv7") && (arch != "armv6")) {
        Core.raise(ValueError("UnexpectedArchitecture"));
    }

    gpioDrivers = [
        "drivers/gpio/broadcom/bc27:bc27gpio",
        "drivers/gpio/rockchip/rk32:rk32gpio",
        "drivers/gpio/ti/omap4:om4gpio",
    ];

    entries = group("gpio_drivers", gpioDrivers);
    return entries;
}

