/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    SD

Abstract:

    This module implements the Secure Digital host controller driver. It
    can be used as a standalone driver or imported as a support library.

Author:

    Evan Green 27-Feb-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "sd";
    var sources;

    sources = [
        "sd.c",
        "sdlib.c",
        "sdstd.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

