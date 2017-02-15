/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Partition

Abstract:

    This module implements the partition support driver.

Author:

    Evan Green 30-Jan-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var libs;
    var name = "part";
    var sources;

    sources = [
        "part.c"
    ];

    libs = [
        "lib/partlib:partlib"
    ];

    drv = {
        "label": name,
        "inputs": sources + libs,
    };

    entries = driver(drv);
    return entries;
}

