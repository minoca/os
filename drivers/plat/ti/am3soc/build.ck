/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    AM335x SoC

Abstract:

    This module implements the TI AM335x SoC support driver.

Author:

    Evan Green 9-Sep-2015

Environment:

    Kernel

--*/

from menv import addConfig, driver;

function build() {
    var drv;
    var entries;
    var fwName;
    var name = "am3soc";
    var sources;

    fwName = "am3cm3fw";
    sources = [
        "am3soc.c",
        "am3cm3fw.S",
        "mailbox.c",
        "sleep.S"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    for (entry in entries) {
        if ((entry.get("output")) && (entry.output.endsWith("am3cm3fw.o"))) {
            addConfig(entry, "CPPFLAGS", "-I$S/drivers/plat/ti/am3soc");
            break;
        }
    }

    return entries;
}

