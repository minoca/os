/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Google EC

Abstract:

    This directory contains the driver for the Google Embedded Controller.

Author:

    Evan Green 25-Aug-2015

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var dynlibs;
    var entries;
    var name = "goec";
    var sources;

    sources = [
        "goec.c",
        "keymap.c"
    ];

    dynlibs = [
        "drivers/input/usrinput:usrinput"
    ];

    drv = {
        "label": name,
        "inputs": sources + dynlibs,
    };

    entries = driver(drv);
    return entries;
}

