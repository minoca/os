/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    Quark Host Bridge

Abstract:

    This module implements the Quark Host Bridge driver, which includes
    support for features like IMRs (Isolated Memory Regions).

Author:

    Evan Green 4-Dec-2014

Environment:

    Kernel

--*/

from menv import driver;

function build() {
    var drv;
    var entries;
    var name = "qrkhostb";
    var sources;

    sources = [
        "qrkhostb.c"
    ];

    drv = {
        "label": name,
        "inputs": sources,
    };

    entries = driver(drv);
    return entries;
}

